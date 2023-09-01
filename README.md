# Setup

To successfully build this project, carefully follow these steps:

1. **Install ESP-IDF:**
   Ensure that you have the ESP-IDF (Espressif IoT Development Framework) correctly installed on your system.

2. **Select a Specific Version:**
   Navigate to the ESP-IDF repository and choose the desired version by executing the following command:

   ```bash
   git checkout v4.4.5
   ```

3. **Clone the Project Repository:**
   Clone the project repository using SSH with this command:

   ```bash
   git clone git@github.com:yourusername/keybox-core.git
   ```

4. **Initialize Submodules:**
   Once the repository is cloned, navigate to the project directory and initialize any submodules it may contain:

   ```bash
   cd keybox-core
   git submodule update --init --recursive
   ```

5. **Configure Wi-Fi and Golioth Cloud Credentials:**
   Access the ESP-IDF menu configuration by running the following command:

   ```bash
   idf.py menuconfig
   ```

   Within the menu configuration interface, navigate to `Component Config` and then select `Credentials` (located at the bottom of the page).

   After entering the correct credentials, press 'Q' to exit the configuration, and it will prompt you to save your changes.

6. **Build and Flash the Project:**
   To build and flash the project, execute the following command:

   ```bash
   idf.py build flash
   ```

7. **Optional: Monitor Logs:**
   If you wish to monitor the logs, use the following command:

   ```bash
   idf.py monitor
   ```

For additional configuration options and in-depth guidance, consult the ESP-IDF manual.
