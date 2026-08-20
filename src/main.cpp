#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "Router.hpp"
#include "Server.hpp"

int main(){
    // Router Defined here
    Router router;

    router.get(
        "/",
        [](const HTTPRequest&){
            HTTPResponse response;
            response.setStatus(202, "OK");
            response.setHeader("Content-Type", "text/plain");
            response.setBody("This is the home page");

            return response;
        }
    );

    router.get(
        "/about",
        [](const HTTPRequest&){
            HTTPResponse response;
            response.setStatus(202, "OK");
            response.setHeader("Content-Type", "text/plain");
            response.setBody("This is the about page");

            return response;
        }
    );

    router.get(
        "/help",
        [](const HTTPRequest&){
            HTTPResponse response;
            response.setStatus(202, "OK");
            response.setHeader("Content-Type", "text/plain");
            response.setBody("This is the help page");

            return response;
        }
    );

    router.post(
        "/login",
        [](const HTTPRequest&)
        {
            HTTPResponse response;
            response.setBody("Login Successful");

            return response;
        }
    );

    Server server(8080);

    server.setRouter(router);

    server.start();

    return 0;
}