#include <iostream>
#include <cpptk/cpptk.h>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/client.hpp>
#include <mutex>
#include <tcl.h>

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
        label(".login") - text("Welcome to infight!");
        pack(".login") - pady(10) - padx(5);
        label(".username") - text("Username:");
        pack(".username") - pady(10) - padx(5);
        entry(".nentry");
        pack(".nentry");
        label(".password") - text("Password:");
        pack(".password") - pady(10) - padx(5);
        entry(".npassword");
        pack(".npassword");
        button(".loginbutton") - command(std::bind(&Infight::login_callback, this)) - text("Login");
        pack(".loginbutton") - pady(10) - padx(5);
    }

    void on_message(websocketpp::client<websocketpp::config::asio_tls> *client, websocketpp::connection_hdl hdl, websocketpp::config::asio_tls::message_type::ptr msg) {

    }

    void on_connection(websocketpp::connection_hdl hdl) {
        std::mutex mtx;
        mtx.lock();
        ".test" << configure() - text("Connected to Discord Gateway!");
        mtx.unlock();
    }

    void login_callback() {
        destroy(".loginbutton");
        label(".test") - text("Establishing connection!");
        pack(".test") - pady(10) - padx(5);
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
            ".login" << configure() - text("Failed to initialize networking!");
            destroy(".test");
            button(".loginbutton") - command(std::bind(&Infight::login_callback, this)) - text("Login");
            pack(".loginbutton") - pady(10) - padx(5);
            return;
        }
        ".test" << configure() - text("Sucessfully initialized networking.");
        client.connect(con);
        launch_asio_event_loop();
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

private:
    websocketpp::client<websocketpp::config::asio_tls> client;
    std::thread remote;
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
