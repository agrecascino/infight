#include <iostream>
#include <cpptk/cpptk.h>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/client.hpp>
#include <mutex>
#include <tcl.h>
#include <jsoncpp/json/json.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#include <experimental/filesystem>
#include <fstream>

using namespace std;
using namespace Tk;

class Window {
public:
    Window(int xsz, int ysz, string ti) {
        std::string fixed_title = "." + ti;
        std::cout << fixed_title << std::endl;
        std::replace(fixed_title.begin(), fixed_title.end(), ' ', '_');
        toplevel(fixed_title) -width(xsz) -height(ysz);
        t = title;
        ft = fixed_title;
        wm("title", fixed_title, ti);
    }

    string get_internal_name() {
        return ft;
    }

    virtual void draw() {}

protected:
    int x;
    int y;
    string ft;
    string t;
};

class List : public Window {
public:
    List(int xsz, int ysz, string ti) : Window(xsz, ysz, ti) {}
    virtual void draw() {
        int i = 0;
        for(std::string s : items){
            label(ft + "." + to_string(i)) - text(s);
            pack(ft + "." + to_string(i))  - padx(5) - pady(6);
            i++;
        }
        if(!items.size()) {
            label(ft + ".empty") - text("Nothing here!");
            pack(ft + ".empty")  - padx(5) - pady(6);
        }
    }

    void set_list(std::vector<std::string> list) {
        items = list;
    }

private:
    std::vector<std::string> items;
};

class RootWindow {
public:
    RootWindow(string title, int x, int y) {
        t = title;
        wm("title", ".", title);
        wm("geometry", ".", to_string(x) + "x" + to_string(y));
    }

    void resize(int x, int y) {
        wm("geometry", ".", to_string(x) + "x" + to_string(y));
    }

    void retitle(string title) {
        wm("title", ".", title);
    }

private:
    string t;
};

class WindowManager {
public:
    WindowManager() : m("Infight", 400, 300) {
    }

    private:
    RootWindow m;
    std::thread t;
};

class Infight {
public:
    Infight() {
        cURLpp::initialize();
        load_spoof();
    }

    void load_spoof(bool retry = false) {
        if (retry) {
           destroy(".failure");
           destroy(".retry");
        }
        try {
            cURLpp::Easy spoofinfo;
            spoofinfo.setOpt<curlpp::options::Url>("https://discordapp.com/");
            spoofinfo.setOpt<curlpp::options::UserAgent>(fakeagent);
            spoofinfo.perform();
            cURLpp::infos::CookieList::get(spoofinfo, spoofcookies);
        } catch(...) {
            label(".failure") - text("Failed to get spoofing info!");
            pack(".failure") - pady(10) - padx(5);
            button(".retry") - command(std::bind(&Infight::load_spoof, this, true)) - text("Retry");
            pack(".retry") - pady(10) - padx(5);
            return;
        }
        create_login_panel();

    }

    void create_login_panel() {
        std::experimental::filesystem::path token(".config/infight/token");
        string line;
        if (std::experimental::filesystem::exists(token)) {
            std::fstream a(std::experimental::filesystem::absolute(token), std::ios_base::in);
            if(std::getline(a, line)) {
                token = line;
            } else {
                goto nevermind;
            }
            label(".login") - text("Token found, connecting...");
            pack(".login") - pady(10) - padx(5);
            gateway_callback_shim();
            return;
        }
        nevermind:
        label(".login") - text("Welcome to infight!");
        pack(".login") - pady(10) - padx(5);
        label(".username") - text("Username:");
        pack(".username") - pady(10) - padx(5);
        entry(".nentry") - textvariable(login_mail);
        pack(".nentry");
        label(".password") - text("Password:");
        pack(".password") - pady(10) - padx(5);
        entry(".npassword") - textvariable(login_pass);
        pack(".npassword");
        button(".loginbutton") - command(std::bind(&Infight::login_callback, this)) - text("Login");
        pack(".loginbutton") - pady(10) - padx(5);
    }

    void on_message(websocketpp::client<websocketpp::config::asio_tls> *client, websocketpp::connection_hdl hdl, websocketpp::config::asio_tls::message_type::ptr msg) {
        std::cout <<
    }

    void on_connection(websocketpp::connection_hdl hdl) {
        std::mutex mtx;
        mtx.lock();
        ".login" << configure() - text("Connected to Discord Gateway!");
        mtx.unlock();
        Json::Value hello;
        hello["heartbeat_interval"] = 42000;
        hello["_trace"] = {""};
        websocketpp::lib::error_code ec;
        client.send(hdl,writer.write(hello), websocketpp::frame::opcode::text, ec);

    }

    Json::Value login(Json::Value root) {
        cURLpp::Easy loginrq;
        std::list<std::string> header;
        header.push_back("Content-Type: application/json");
        loginrq.setOpt<curlpp::options::Url>(authurl);
        loginrq.setOpt<curlpp::options::UserAgent>(fakeagent);
        loginrq.setOpt<curlpp::options::HttpHeader>(header);
        for(string s : spoofcookies) {
            loginrq.setOpt<curlpp::options::CookieList>(s);
        }
        std::string comb = writer.write(root);
        std::cout << comb << std::endl;
        loginrq.setOpt(new curlpp::options::PostFields(comb));
        loginrq.setOpt(new curlpp::options::PostFieldSize(comb.length()));
        std::cout << login_mail << ":" << login_pass << std::endl;
        std::ostringstream response;
        loginrq.setOpt(new curlpp::options::WriteStream(&response));
        loginrq.perform();
        std::cout << response.str() << std::endl;
        Json::Value token;
        Json::Reader reader;
        reader.parse(response.str(), token);
        for(string s : token.getMemberNames()) {
            std::cout << s << std::endl;
        }
        std::cout << token.toStyledString() << std::endl;
        return token;
    }

    int gateway_connect() {
        client.set_open_handler(std::bind(&Infight::on_connection, this, std::placeholders::_1));
        client.set_message_handler(std::bind(&Infight::on_message, this, &client, std::placeholders::_1, std::placeholders::_2));
        client.clear_access_channels(websocketpp::log::alevel::message_payload);
        websocketpp::lib::error_code ec;
        client.init_asio();
        client.set_tls_init_handler([this](websocketpp::connection_hdl){
            return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);
        });
        websocketpp::client<websocketpp::config::asio_tls>::connection_ptr con = client.get_connection("wss://gateway.discord.gg", ec);
        if(ec) {
            return 0;
        }
        client.connect(con);
        launch_asio_event_loop();
        return 1;
    }

    void gateway_callback_shim() {
        if(!gateway_connect()) {
            button(".rebutton") - command(std::bind(&Infight::gateway_callback_shim, this)) - text("Login");
            pack(".rebutton") - pady(10) - padx(5);
        }
    }

    void login_callback() {
        destroy(".loginbutton");
        Json::Value root;
        root["email"] = login_mail;
        root["password"] = login_pass;
        std::cout << login_mail << ":" << login_pass << std::endl;
        Json::Value token = login(root);
        try{
            this->token = token["token"].asString();
            std::experimental::filesystem::path p(".config/infight/token");
            std::string path = std::experimental::filesystem::absolute(p);
            std::fstream f(path, std::ios_base::out | std::ios_base::trunc);
            f.write(token["token"].asString().c_str(), token["token"].asString().size());
            f.close();
        } catch(...) {
            ".login" << configure() - text("Failed to login to discord!");
            button(".loginbutton") - command(std::bind(&Infight::login_callback, this)) - text("Login");
            pack(".loginbutton") - pady(10) - padx(5);
            return;
        }
        destroy(".username");
        destroy(".nentry");
        destroy(".password");
        destroy(".npassword");
        gateway_callback_shim();
    }

    void launch_asio_event_loop() {
        remote = std::thread([&]() {
            asio_event_loop();
        });
        remote.detach();
    }

    void asio_event_loop() {
        client.run();
    }

    void heartbeat_loop(size_t ms) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));

        }
    }

private:
    Json::FastWriter writer;
    std::string token;
    std::string login_mail;
    std::string login_pass;
    std::list<std::string> spoofcookies;
    const std::string fakeagent = "Mozilla/5.0 (X11; Linux x86_64; rv:52.0) Gecko/20100101 Firefox/52.0";
    const std::string authurl = "https://discordapp.com/api/v6/auth/login";
    websocketpp::client<websocketpp::config::asio_tls> client;
    std::thread remote;
    std::thread heartbeat;
    WindowManager manager;
};

int main(int argc, char *argv[])
{
    init(argv[0]);
    Infight infight;
    runEventLoop();
    //Infight infight;
    //infight.event_loop();
    return 0;
}
