#include <iostream>
#include <string>
#include <netdb.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/beast/http.hpp>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include <boost/program_options.hpp>


namespace po = boost::program_options;


#include <BoostLogging.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;


class RESTServer{
private:
    void accept(){
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                accept();
            }
        );
    }

    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket) : socket_(std::move(socket)) {

        }

        void start(){
            read_request();
        }

    private:
        struct _ApiResponse{
            http::status status;
            std::string code;
            std::string message;

            std::string toJson(){
                boost::property_tree::ptree newPt;
                newPt.put<int>("status", static_cast<int>(status));
                newPt.put("code", code);
                newPt.put("message", message);

                std::ostringstream oss;
                boost::property_tree::write_json(oss, newPt);
                return oss.str();
            }
        };
        typedef struct _ApiResponse ApiResponse;

        ApiResponse make_api_response(http::status status, std::string code, std::string message){
            return ApiResponse{status, code, message};
        }

        void read_request() {
            auto self = shared_from_this();
            beast::http::async_read(
                socket_, buffer_, request_,
                [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    if (!ec) {
                        self->process_request(self->request_); // self 포인터를 사용하여 멤버 함수 호출
                    }
                }
            );
        }

        void process_request(const http::request<http::string_body>& request_);
        void write_response(http::status staus, std::string body="") {
            response_.set(http::field::content_type, "application/json");

            response_.result(staus);
            if (body.length() >= 0){
                response_.body() = body;
            }
            
            auto self = shared_from_this();
            beast::http::async_write(
                socket_, response_,
                [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                }
            );
        };

        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;
        http::response<http::string_body> response_;
    };

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;

    BoostLogger boostlogger;


public:
    RESTServer(asio::io_context& io_context, const tcp::endpoint& endpoint)
            : io_context_(io_context), acceptor_(io_context, endpoint) {
        logging::core::get()->set_filter(
            logging::trivial::severity >= logging::trivial::debug
        );
    }
    ~RESTServer(){

    }

    void run(){
        accept();
        io_context_.run();
    }
};




void RESTServer::Session::process_request(const http::request<http::string_body>& request_) {
    response_.version(request_.version());
            
    std::string path = request_.target();
    std::string body = request_.body();

    for (const auto& header : request_.base()){
        _TRACE_LOG_(global_lg::get(), debug, "%s:%s", std::string(header.name_string()).c_str(), std::string(header.value()).c_str());
    }

    if (request_.method() == http::verb::post) {
        
        if(path == "/v1/token"){
            std::string clientId = request_["X-Client-Id"];
            std::string clientPasswordd = request_["X-Client-Password"];
            if (clientId.length() <= 0){
                write_response(
                    http::status::bad_request,
                    make_api_response(
                        http::status::bad_request,
                        "90003",
                        "X-Client-Id not set"
                    ).toJson()
                );
            } else if (clientPasswordd.length() <= 0){
                write_response(
                    http::status::bad_request,
                    make_api_response(
                        http::status::bad_request,
                        "90004",
                        "X-Client-Password not set"
                    ).toJson()
                );
            } else {
                _TRACE_LOG_(global_lg::get(), info, "(request) path:%s clientId:%s, clientPassword:%s", path.c_str(), clientId.c_str(), clientPasswordd.c_str());
                write_response(
                    http::status::ok
                );
            }
        } else {
            std::string message("No handler found for " + std::string(request_.method_string()) + " " + std::string(request_.target()));
            write_response(
                http::status::not_found,
                make_api_response(
                    http::status::not_found,
                    "90001",
                    message
                ).toJson()
            );
        }
    } else {
        _TRACE_LOG_(global_lg::get(), error, "(unknown) method:%s body:%s", std::string(request_.method_string()).c_str(), body.c_str());
        
        write_response(
            http::status::method_not_allowed,
            make_api_response(
                http::status::method_not_allowed,
                "90002",
                "Method Not Allowed"
            ).toJson()
        );
    }
}



int main(int argc, char **argv) {
    int port;

    po::options_description options("Options");
    options.add_options()
        ("help",     "produce this help message")
        ("port,p",  po::value<int>(&port)->required(), 
                       "listening port")
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

    try {

        asio::io_context io_context;
        tcp::endpoint endpoint(tcp::v4(), port);
        RESTServer server(io_context, endpoint);

        std::thread server_thread([&]() {
            server.run();
        });

        server_thread.join();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
