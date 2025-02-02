#pragma once
#include <string>
#include <QNetworkAccessManager>

struct ClashConfig
{
    std::string external_controller;
    std::string secret;
};

class Skipper
{
public:
    static std::unique_ptr<Skipper> create();
    void skip();
    ~Skipper();

private:
    explicit Skipper(ClashConfig);

    ClashConfig config;
    QNetworkAccessManager* manager;
};
