# AWS Lambda C++20 (Docker-based Build)

This project demonstrates how to build and deploy an AWS Lambda function written in C++20 using Docker and the AWS Lambda C++ runtime.

## I - Prerequisites

Before getting started, make sure you have installed:

- Docker 🐳 (required to build the Lambda binary in a compatible environment)
   - https://docs.docker.com/get-docker/

- AWS CLI (configured with your credentials)
    - https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html

## II - Project Structure

- build.sh        # Build + package + deploy script
- Dockerfile      # Build environment (Amazon Linux 2023)
- main.cpp        # Lambda handler (C++20)
- bootstrap.zip   # Generated deployment package

## III - How It Works

1. Docker builds a Lambda-compatible binary (bootstrap)
2. The binary is copied from the container to your host
3. The binary is zipped into bootstrap.zip
4. The zip is uploaded to AWS Lambda

## IV - Build & Deploy

Make the script executable:
- chmod +x build.sh

Run the deployment:
- ./build.sh <function-name>

Example:
- ./build.sh fb-cpp20-poc

## V - Notes

The output binary must be named bootstrap for AWS Lambda custom runtimes.

The build is done inside Docker to ensure compatibility with AWS Lambda's environment.

C++20 is enabled via:

- -std=c++20

## VI - Troubleshooting

Docker not found → install Docker and ensure it's running

AWS CLI errors → check aws configure

Permission issues → ensure build.sh is executable

# VII - Result

After running the script, your Lambda function will be updated with the new C++20 binary and ready to invoke.

Example:
- aws lambda invoke --function-name fb-cpp20-poc output.txt
