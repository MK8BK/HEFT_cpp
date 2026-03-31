#!/bin/bash

FUNCTION_NAME="$1"

docker build -t cpp-lambda .
docker run -it --rm -v $(pwd):/var/task/tmp cpp-lambda cp /var/task/bootstrap /var/task/tmp 

zip bootstrap.zip bootstrap

aws lambda update-function-code  --function-name "$FUNCTION_NAME" \
  --zip-file fileb://bootstrap.zip
