#include <iostream>
#include <opencv2/opencv.hpp>
#include <cmath>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <fstream>

//test

cv::Mat preprocessImage(const cv::Mat& img, int downsamplingFactor) {
    if (img.empty()) {
        throw std::runtime_error("Input image is empty.");
    }

    cv::Mat processedImage = img.clone();

    // Mindestgröße prüfen
    if (processedImage.rows >= 250 && processedImage.cols >= 250) {
        const int cropSize = 25;
        const int margin = 5;

        int midX = processedImage.cols / 2;
        int midY = processedImage.rows / 2;

        // Fünf Regionen definieren
        std::vector<cv::Rect> regions = {
            cv::Rect(midX - cropSize / 2,           midY - cropSize / 2,           cropSize, cropSize),
            cv::Rect(midX - cropSize - cropSize / 2,  midY - cropSize - cropSize / 2,  cropSize, cropSize),
            cv::Rect(midX + cropSize / 2,           midY - cropSize - cropSize / 2,  cropSize, cropSize),
            cv::Rect(midX - cropSize - cropSize / 2,  midY + cropSize / 2,           cropSize, cropSize),
            cv::Rect(midX + cropSize / 2,           midY + cropSize / 2,           cropSize, cropSize)
        };

        // Stitched-Image erzeugen
        cv::Mat stitchedImage(cropSize, cropSize * 5 + margin * 4, img.type(), cv::Scalar(0));
        for (size_t i = 0; i < regions.size(); ++i) {
            cv::Mat cropped = processedImage(regions[i]);
            int xOffset = static_cast<int>(i) * (cropSize + margin);
            cv::Rect destRegion(xOffset, 0, cropSize, cropSize);
            cropped.copyTo(stitchedImage(destRegion));
        }
        processedImage = stitchedImage.clone();

        // Downsampling
        /*cv::resize(processedImage, processedImage,
            cv::Size(processedImage.cols / downsamplingFactor,
                processedImage.rows / downsamplingFactor));*/
    }

    // In 8U konvertieren
    cv::Mat imgUInt8;
    if (processedImage.type() != CV_8U) {
        processedImage.convertTo(imgUInt8, CV_8U);
    }
    else {
        imgUInt8 = processedImage.clone();
    }

    // Histogrammausgleich
    cv::Mat equalizedImg;
    cv::equalizeHist(imgUInt8, equalizedImg);

    cv::Mat sobelx, sobely;
    cv::Sobel(equalizedImg, sobelx, CV_64F, 1, 0, 5);
    cv::Sobel(equalizedImg, sobely, CV_64F, 0, 1, 5);


    cv::Mat sobel;
    cv::magnitude(sobelx, sobely, sobel);
    cv::normalize(sobel, sobel, 0, 255, cv::NORM_MINMAX);
    sobel.convertTo(sobel, CV_8U);

    return sobel;
}


double CalcMetricsDense(const cv::Mat& img1_gray,
    const cv::Mat& img2_gray,
    double pixel_to_cm_ratio,
    double pyr_scale,
    int levels,
    int winsize,
    int iterations,
    int poly_n,
    double poly_sigma,
    int flags,
    int downsamplingFactor) {
    static cv::Mat last_preprocessed;

    cv::Mat img2_preprocessed = preprocessImage(img2_gray, downsamplingFactor);

    cv::Mat img1_preprocessed;
    if (last_preprocessed.empty()) {
        img1_preprocessed = preprocessImage(img1_gray, downsamplingFactor);
    }
    else {
        img1_preprocessed = last_preprocessed;
    }

    cv::Mat flow;
    cv::calcOpticalFlowFarneback(img1_preprocessed, img2_preprocessed, flow,
        pyr_scale, levels, winsize,
        iterations, poly_n, poly_sigma, flags);

    if (flow.empty() || flow.type() != CV_32FC2) {
        std::cerr << "Error: Optical flow computation failed. Returning fallback value.\n";
        last_preprocessed = img2_preprocessed;
        return 0.0;
    }

    std::vector<cv::Mat> channels(2);
    cv::split(flow, channels);
    cv::Mat flow_x = channels[0];
    cv::Mat flow_y = channels[1];

    cv::Mat magnitude, angle;
    cv::cartToPolar(flow_x, flow_y, magnitude, angle);

    std::vector<float> magValues(magnitude.total());
    for (int i = 0; i < magnitude.rows; ++i) {
        const float* rowPtr = magnitude.ptr<float>(i);
        for (int j = 0; j < magnitude.cols; ++j) {
            magValues[i * magnitude.cols + j] = rowPtr[j];
        }
    }

    if (magValues.empty()) {
        std::cerr << "Error: No magnitude values. Returning fallback.\n";
        last_preprocessed = img2_preprocessed;
        return 0.0;
    }

    float thresh = static_cast<float>(std::max(magnitude.rows, magnitude.cols)) * 0.5f;
    magValues.erase(std::remove_if(magValues.begin(), magValues.end(),
        [thresh](float val) { return val > thresh; }),
        magValues.end());
    if (magValues.empty()) {
        std::cerr << "Error: All magnitude values removed. Returning fallback.\n";
        last_preprocessed = img2_preprocessed;
        return 0.0;
    }

    std::nth_element(magValues.begin(),
        magValues.begin() + magValues.size() / 2,
        magValues.end());
    double median_length_pixels = magValues[magValues.size() / 2];
    double median_length_cm = median_length_pixels / pixel_to_cm_ratio;

    last_preprocessed = img2_preprocessed;
    return median_length_cm;
}


double CalcMetricsSparse(const cv::Mat& img1_gray,
    const cv::Mat& img2_gray,
    double pixel_to_cm_ratio,
    int downsamplingFactor) {
    static cv::Mat last_preprocessed;

    cv::Mat img2_preprocessed = preprocessImage(img2_gray, downsamplingFactor);

    cv::Mat img1_preprocessed;
    if (last_preprocessed.empty()) {
        img1_preprocessed = preprocessImage(img1_gray, downsamplingFactor);
    }
    else {
        img1_preprocessed = last_preprocessed;
    }

    std::vector<cv::Point2f> p0;
    cv::goodFeaturesToTrack(img1_preprocessed, p0, 200, 0.2, 3, cv::Mat(), 11);
    if (p0.empty()) {
        std::cerr << "Warning: No features. Returning fallback.\n";
        last_preprocessed = img2_preprocessed;
        return 0.0;
    }

    std::vector<cv::Point2f> p1;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(img1_preprocessed, img2_preprocessed, p0, p1,
        status, err, cv::Size(15, 15), 2,
        cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 10, 0.03));

    std::vector<cv::Point2f> good_new, good_old;
    for (size_t i = 0; i < status.size(); i++) {
        if (status[i] == 1 && p1[i].x >= 0 && p1[i].y >= 0 && p0[i].x >= 0 && p0[i].y >= 0) {
            good_new.push_back(p1[i]);
            good_old.push_back(p0[i]);
        }
    }
    if (good_new.empty()) {
        std::cerr << "Warning: No valid matches. Returning fallback.\n";
        last_preprocessed = img2_preprocessed;
        return 0.0;
    }


    std::vector<double> vector_lengths;
    double threshold = std::max(img1_preprocessed.cols, img1_preprocessed.rows) * 0.5;
    for (size_t i = 0; i < good_new.size(); i++) {
        cv::Point2f mv = good_new[i] - good_old[i];
        double length = std::sqrt(mv.x * mv.x + mv.y * mv.y);
        if (length < threshold) {
            vector_lengths.push_back(length);
        }
    }
    if (vector_lengths.empty()) {
        std::cerr << "Warning: No vectors after outlier removal. Returning fallback.\n";
        last_preprocessed = img2_preprocessed;
        return 0.0;
    }

    std::nth_element(vector_lengths.begin(),
        vector_lengths.begin() + vector_lengths.size() / 2,
        vector_lengths.end());
    double median_length_pixels = vector_lengths[vector_lengths.size() / 2];
    double median_length_cm = median_length_pixels / pixel_to_cm_ratio;

    last_preprocessed = img2_preprocessed;
    return median_length_cm;
}





using json = nlohmann::json;

int main(int argc, char* argv[]) {
    // Default values
    int image_width = 1920;
    int image_height = 1080;
    bool snapshot = false;

    // Parse arguments
    if (argc > 1) {
        image_width = std::stoi(argv[1]); // First argument: image width
    }
    if (argc > 2) {
        image_height = std::stoi(argv[2]); // Second argument: image height
    }
    if (argc > 3) {
        std::string snapshot_arg = argv[3];
        snapshot = (snapshot_arg == "true" || snapshot_arg == "1"); // Third argument: snapshot
    }

    // Print parsed values
    std::cout << "Image Width: " << image_width << "\n";
    std::cout << "Image Height: " << image_height << "\n";
    std::cout << "Snapshot: " << (snapshot ? "true" : "false") << "\n";

    // Default variable declarations
    double pixel_to_cm_ratio = 1;
    double pyr_scale = 0.5;
    int levels = 5;
    int winsize = 15;
    int iterations = 1;
    int poly_n = 5;
    double poly_sigma = 1.2;
    int downsamplingFactor = 1;

    // Read from JSON
    std::string config_file = "config.json";
    json config;

    try {
        std::ifstream file(config_file);
        if (file.is_open()) {
            file >> config;

            // Update variables from JSON
            if (config.contains("pixel_to_cm_ratio"))
                pixel_to_cm_ratio = config["pixel_to_cm_ratio"];
            if (config.contains("pyr_scale"))
                pyr_scale = config["pyr_scale"];
            if (config.contains("levels"))
                levels = config["levels"];
            if (config.contains("winsize"))
                winsize = config["winsize"];
            if (config.contains("iterations"))
                iterations = config["iterations"];
            if (config.contains("poly_n"))
                poly_n = config["poly_n"];
            if (config.contains("poly_sigma"))
                poly_sigma = config["poly_sigma"];
            if (config.contains("downsamplingFactor"))
                downsamplingFactor = config["downsamplingFactor"];
        } else {
            throw std::runtime_error("Could not open config.json");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading config.json: " << e.what() << "\n";
        return 1;
    }

    // Print loaded configuration
    std::cout << "Loaded configuration:\n";
    std::cout << "Pixel to cm ratio: " << pixel_to_cm_ratio << "\n";
    std::cout << "Pyramid scale: " << pyr_scale << "\n";
    std::cout << "Levels: " << levels << "\n";
    std::cout << "Window size: " << winsize << "\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Poly N: " << poly_n << "\n";
    std::cout << "Poly Sigma: " << poly_sigma << "\n";
    std::cout << "Downsampling Factor: " << downsamplingFactor << "\n";

    cv::VideoCapture cap(0, cv::CAP_V4L2); 
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, image_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, image_height);
    cap.set(cv::CAP_PROP_FPS, 30);

    if (!cap.isOpened()) {
        std::cerr << "Error: Unable to open the camera!" << std::endl;
        return -1;


    }

    if (snapshot) {
        cv::Mat frame;
        cap >> frame;

        if (frame.empty()) {
            std::cerr << "Error: Could not grab a frame.\n";
            return -1;
        }

        // Save the frame
        cv::imwrite("measure.png", frame);
        std::cout << "Snapshot saved as measure.png.\n";

        return 0; // Exit the program
    }





    cv::Mat frame_old;
    {
        cv::Mat initialFrame;
        cap >> initialFrame;
        if (initialFrame.empty()) {
            throw std::runtime_error("Failed to capture initial frame.");
        }

        cv::cvtColor(initialFrame, frame_old, cv::COLOR_BGR2GRAY);
    }

    int i = 0;
    cv::Mat frame;
    cv::Mat grayFrame;
    double median_length_cm;


    while (true) {
        auto capture_start = std::chrono::high_resolution_clock::now();
        cap >> frame;

        cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

        // Example: Dense-Flow
        // median_length_cm = CalcMetricsSparse(frame_old, grayFrame, pixel_to_cm_ratio, downsamplingFactor);
        median_length_cm = CalcMetricsDense(frame_old, grayFrame, pixel_to_cm_ratio, pyr_scale, levels, winsize, iterations, poly_n, poly_sigma, 0, downsamplingFactor);

        auto capture_end = std::chrono::high_resolution_clock::now();
        
        // Correctly calculate time difference in seconds
        std::chrono::duration<double> timeDif = capture_end - capture_start;

        std::cout << "Iteration " << i++
                << " - Median length: " << median_length_cm << " cm | Time: "
                << timeDif.count() << " s | FPS: "
                << (1.0 / timeDif.count()) << std::endl;

        frame_old = grayFrame;
    }


    return 0;
}
