
# OptiFlow

## Step 1: Install Docker on Host
### On Host:
#### Add Docker's official GPG key:
sudo apt-get update
sudo apt-get install ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

#### Add the repository to Apt sources:
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update

### Install Docker and Utils
### On Host:
$sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

## Step 2: Clone this Repo
### On Host:
$ git clone https://github.com/DenizEDRS/OptiFlow.git

## Step 3: Allow GUI for container
### On Host:
$sudo apt install x11-xserver-utils
$xhost +local:docker


## Step 4: Start the Container
### On Host:
$sudo ./build_container.sh
(This may take a while)

## Step 5: Run the Container
### On Host:
$sudo ./run_container.sh


## Step 6: Run guvcview to configure Camera
### In Container:
$guvcview

-> Turn off auto-settings, set Resolution and Framerate
-> Quit after setting everything up

## Step 7: Build the project
### In Container:
(To build the project, just run the alias:
$build_main

## Step 8: Run the Project
### In Container:
Optional: Adjust Parameters in config.json
$./build/red_window 1920 1080 // For Normal Execution at Full HD
$./build/red_window 1920 1080 1 // For Snapshots, to measure the Pixel per CM Ratio
