# HOW TO BUILD

To build the project, follow these steps:

1. **Install ESP-IDF:** Ensure you have the ESP-IDF (Espressif IoT Development Framework) installed on your system.

2. **Select Version:** In the ESP-IDF repository, switch to the desired version by running the following command:

   ```
   git checkout v4.4.5
   ```

3. **Clone the Repository:** Clone the project repository using SSH with the following command:

   ```
   git clone git@github.com:yourusername/your-repo.git
   ```

4. **Initialize Submodules:** Once the repository is cloned, navigate into the project directory and initialize any submodules it may contain:

   ```
   cd your-repo
   git submodule update --init --recursive
   ```

Now you have set up the project correctly and can proceed with the build process.
