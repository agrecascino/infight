#include <iostream>
#include <cpptk/cpptk.h>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/client.hpp>
#include <mutex>
#include <tcl.h>
#include <json/json.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <map>

using namespace std;
using namespace Tk;
struct User {
    std::string userid;
    std::string displayname;
    std::string discriminator;
};

struct Message {
    std::string content;
    User author;
    std::string id;
    virtual std::string construct() { return author.displayname + ": " + content + "\n"; }
};

struct NotAMessage : public Message {
    virtual std::string construct() { return content; }
};

class MessageLogger {
    void PushMessage(Message *msg) {
        messages.push_back(msg);
    }

    Message* getLastMessage() {
        return messages.back();
    }

    Message* getMessageByIndex(size_t i) {
        return messages[i];
    }

    Message* getMessageById(std::string id) {
        for(Message *msg : messages) {
            if(msg->id == id)
                return msg;
        }
        return nullptr;
    }

private:
    std::vector<Message*> messages;

};

enum DMType {
    GroupDM,
    UserDM
};

struct DMChat {
    DMType type;
    std::string id;
    std::vector<User> users;
    std::string title;
    MessageLogger logger;

    User get_sdmuser() const { return users[0]; }
};

struct Channel {
    std::string id;
    std::string name;
    MessageLogger logger;
};

struct Guild {
    std::string id;
    std::string displayname;
    std::vector<Channel> channels;

    Channel& getChannelById(std::string s) {
        for(Channel &c : channels) {
            if(c.id == s) {
                return c;
            }
        }
        return nullptr;
    }

};

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

    virtual ~Window() {}

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
        listbox(ft + ".lb") -selectmode("multiple") - height(20);
        std::string cmd = "[list " + ft + ".lb set]";
        scrollbar(ft + ".sb") - command(cmd);
        ft + ".lb" << configure() -yscrollcommand("[list " + ft +  ".sb set]");
        ft + ".lb" << insert(0, items.begin(), items.end());
        grid(ft + ".lb", ft + ".sb") - sticky("news");
        if(!items.size()) {
            label(ft + ".empty") - text("Nothing here!");
            pack(ft + ".empty")  - padx(2) - pady(3);
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

enum ConnectStage {
    Unconnected,
    NeedIdentify,
    WaitForReady,
    Connected,
    NeedResume
};

class WindowManager {
public:
    WindowManager() : m("Infight", 400, 300) {
    }

    void AddList(std::string n, List* list) {
        lists[n] = list;
    }

    void RemoveList(std::string n) {
        delete lists[n];
        lists.erase(lists.find(n));
    }

    List* GetList(std::string n) {
        return lists[n];
    }

private:
    std::map<std::string, List*> lists;
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
        std::experimental::filesystem::current_path(getenv("HOME"));
        std::experimental::filesystem::path p(".config/infight/token");
        std::string path = std::experimental::filesystem::absolute(p);
        string line;
        if (std::experimental::filesystem::exists(path)) {
            std::fstream a(std::experimental::filesystem::absolute(path), std::ios_base::in);
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

    void ident() {
        auto hdl = con->get_handle();
        websocketpp::lib::error_code ec;
        Json::Value id;
        Json::Value props;
        id["token"] = token;
        props["$os"] = "linux";
        props["$browser"] = "infight";
        props["$device"] = "infight";
        Json::Value presence;
        presence["status"] = "dnd";
        presence["afk"] = false;
        Json::Value game;
        game["name"] = "nothing";
        game["type"] = 0;
        //presence["since"] = 12345679;
        presence["game"] = game;
        id["presence"] = presence;
        id["properties"] = props;
        Json::Value op;
        op["op"] = 2;
        op["d"] = id;
        client.send(hdl,writer.write(op), websocketpp::frame::opcode::text, ec);
    }

    void on_message(websocketpp::client<websocketpp::config::asio_tls> *client, websocketpp::connection_hdl hdl, websocketpp::config::asio_tls::message_type::ptr msg) {
        std::cout << msg->get_opcode() << std::endl;
        std::cout << msg->get_payload() << std::endl;
        std::mutex mtx;
        mtx.lock();
        Json::Value out;
        Json::Reader reader;
        reader.parse(msg->get_payload(), out);
        int64_t op = out["op"].asInt64();
        Json::Value d = out["d"];
        lastseq = out["s"];
        mtx.unlock();
        if(cstage == NeedIdentify) {
            ident();
            cstage = WaitForReady;
        }
        switch(op) {
        case 10:
            cstage = NeedIdentify;
            launch_heartbeat_event_loop(d["heartbeat_interval"].asInt64());
            break;
        case 11:
            ".login" << configure() - text("ACK recieved" + lastseq.asString());
            break;
        case 1:
            beat();
            break;
        case 0:
            std::string t = out["t"].asString();
            if(t == "READY") {
                cstage = Connected;
                ".login" << configure() - text("Logged in successfully!");
                Json::Value user = d["user"];
                Json::Value dm = d["private_channels"];
                label(".id") - text(user["id"].asString());
                pack(".id") - pady(10) - padx(5);
                label(".username") - text(user["username"].asString());
                pack(".username") - pady(10) - padx(5);
                label(".email") - text(user["email"].asString());
                pack(".email") - pady(10) - padx(5);
                label(".discriminator") - text(user["discriminator"].asString());
                pack(".discriminator") - pady(10) - padx(5);
                List *l = new List(320, 640, "dms");
                std::vector<std::string> names;
                for(Json::Value v : dm) {
                    if(v["type"].asInt64() == 1) {
                        DMChat c;
                        c.type = UserDM;
                        User u;
                        u.userid = v["recipients"][0]["id"].asString();
                        u.displayname = v["recipients"][0]["username"].asString();
                        u.discriminator = v["recipients"][0]["discriminator"].asString();
                        c.users.push_back(u);
                        c.id = v["id"].asString();
                        dms.push_back(c);
                        continue;
                    }
                    DMChat c;
                    c.type = GroupDM;
                    for(Json::Value a : v["recipients"]) {
                        User u;
                        u.userid = a["id"].asString();
                        u.displayname = a["username"].asString();
                        u.discriminator = a["discriminator"].asString();
                        c.users.push_back(u);
                    }
                    c.title = v["name"].asString();
                    if(c.title == "") {
                        c.title = "DM: ";
                        for(User &u : c.users) {
                            c.title += u.displayname;
                        }
                    }
                }

                l->draw();
                List *g = new List(320, 640, "guilds");
                manager.AddList("dms", l);
            }
            break;
        }
    }

    void on_connection(websocketpp::connection_hdl hdl) {
        std::mutex mtx;
        mtx.lock();
        ".login" << configure() - text("Connected to Discord Gateway!");
        mtx.unlock();
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


    void on_fail(websocketpp::connection_hdl hdl) {
        ".login" << configure() - text("Failed to connect to discord gateway!");
        button(".rebutton") - command(std::bind(&Infight::gateway_callback_shim, this)) - text("Login");
        pack(".rebutton") - pady(10) - padx(5);
        client.stop();
    }

    int gateway_connect() {
        client.set_open_handler(std::bind(&Infight::on_connection, this, std::placeholders::_1));
        client.set_message_handler(std::bind(&Infight::on_message, this, &client, std::placeholders::_1, std::placeholders::_2));
        client.set_fail_handler(std::bind(&Infight::on_fail, this, std::placeholders::_1));
        client.clear_access_channels(websocketpp::log::alevel::message_payload);
        websocketpp::lib::error_code ec;
        client.init_asio();
        client.set_tls_init_handler([this](websocketpp::connection_hdl){
            return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);
        });
        con = client.get_connection("wss://gateway.discord.gg/?v=6&encoding=json", ec);
        if(ec) {
            return 0;
        }
        client.connect(con);
        launch_asio_event_loop();
        return 1;
    }

    void gateway_callback_shim() {
        if(!gateway_connect()) {
            button(".rebutton") - command(std::bind(&Infight::gateway_callback_shim, this)) - text("Attempt Reconnect");
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
            std::experimental::filesystem::current_path(getenv("HOME"));
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

    void launch_heartbeat_event_loop(size_t ms) {
        heartbeat = std::thread([&]() {
            heartbeat_loop(ms);
        });
        heartbeat.detach();
    }

    void asio_event_loop() {
        client.run();
    }

    void heartbeat_loop(size_t ms) {
        while (true) {
            beat();
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }

    void beat() {
        auto hdl = con->get_handle();
        websocketpp::lib::error_code ec;
        Json::Value heartbeat;
        heartbeat["op"] = 1;
        heartbeat["d"] = lastseq;
        client.send(hdl,writer.write(heartbeat), websocketpp::frame::opcode::text, ec);
    }

private:
    std::vector<Guild> guilds;
    std::vector<DMChat> dms;
    ConnectStage cstage = Unconnected;
    websocketpp::client<websocketpp::config::asio_tls>::connection_ptr con;
    Json::Value lastseq;
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
