#include <aws/lambda-runtime/runtime.h>
#include <iostream>

int main() {
    auto handler = [](aws::lambda_runtime::invocation_request const& req) {
        return aws::lambda_runtime::invocation_response::success(
            "{\"message\": \"Hello from C++20!\"}", "application/json");
    };

    aws::lambda_runtime::run_handler(handler);
    return 0;
}
