#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <boost/program_options.hpp>


namespace po = boost::program_options;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

class RestClient {
public:
    RestClient(const std::string& _host, const std::string& _port)
        : host(_host), port(_port), resolver(ioContext), socket(ioContext) {

    }

    void requestToken(const std::string clientId, const std::string clientPassword) {
        
        auto const results = resolver.resolve(host, port);

        asio::connect(socket, results.begin(), results.end());

        http::request<http::string_body> request(http::verb::post, "/v1/token", 11);
        request.set(http::field::user_agent, "RestClient/1.0");
        request.set(boost::beast::http::field::accept, "*/*");
		request.set(boost::beast::http::field::connection, "close");
        request.set(boost::beast::http::field::host, host+":"+port);
        request.set("X-Client-id", clientId);
        request.set("X-Client-Password", clientPassword);

        http::write(socket, request);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> response;
        http::read(socket, buffer, response);

        std::string json = boost::beast::buffers_to_string(response.body().data());
        std::cout<<"status:"<<response.result_int()<<std::endl;
        std::cout<<"body:"<<json<<std::endl;
    }

private:
    std::string host;
    std::string port;
    asio::io_context ioContext;
    asio::ip::tcp::resolver resolver;
    asio::ip::tcp::socket socket;
};

int main(int argc, char **argv) {
    std::string host;
    std::string port;

    po::options_description options("Options");
    options.add_options()
        ("help",     "produce this help message")
        ("host,h",  po::value(&host)->required(), 
                       "host")
        ("port,p",  po::value(&port)->required(), 
                       "port")
        ;

    po::variables_map vm;

    try {
        po::store(po::command_line_parser(argc, argv).options(options).run(), vm);
        po::notify(vm);
    }catch (const std::exception& ex) {
        std::cout << "Error parsing options: " << ex.what() << std::endl;
        std::cout << std::endl;
        std::cout << options << std::endl;
        return 1;
    }


    RestClient restClient(host, port);
    try {
        restClient.requestToken("id", "pass");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
