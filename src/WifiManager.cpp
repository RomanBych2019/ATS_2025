#include "WifiManager.h"

#include <EEPROM.h>
#include <WiFi.h>

#include "main.h"

static void buildLoginPage();
static void buildLoginPage(String wifi);
static void action(GyverPortal &p);
static void loginPortal();

void wifiInit()
{
    EEPROM.begin(100);
    EEPROM.get(0, lp);

    // пытаемся подключиться
    Serial.print("Connect to: ");
    Serial.println(lp.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(lp.ssid, lp.pass);
    while (WiFi.status() != WL_CONNECTED && millis() < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("Connected! Local IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        loginPortal();
    }
}

static void loginPortal()
{
    Serial.println("\nStart html Connect Wi-Fi");
    String res{};
    {
        WiFi.disconnect();
        int counterWiFi = WiFi.scanNetworks();
        for (auto i = 0; i < counterWiFi; i++)
        {
            res += WiFi.SSID(i).c_str();
            res += ',';
        }
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    // запускаем портал
    GyverPortal ui;
    ui.attachBuild(buildLoginPage);
    ui.start();
    ui.attach(action);
    // работа портала
    while (ui.tick())
    {
        if (millis() > 60000)
            return;
    }
    Serial.println();
    Serial.println("Close connect Wi-Fi");
}

static void buildLoginPage()
{
    GP.BUILD_BEGIN();
    GP.THEME(GP_DARK);
    GP.FORM_BEGIN("/login");
    GP.TEXT("lg", "Login", lp.ssid);
    GP.BREAK();
    GP.PASS("ps", "Password", lp.pass);
    GP.SUBMIT("Submit");
    GP.FORM_END();
    GP.BUILD_END();
}

static void buildLoginPage(String wifi)
{
    int wifinumder = 0;
    GP.BUILD_BEGIN();
    GP.THEME(GP_DARK);
    GP.FORM_BEGIN("/login");
    GP.SELECT("lg", wifi, wifinumder);
    for (auto i = 0; i < 20; i++)
    {
        lp.ssid[i] = 0;
    }
    for (auto i = 0; i < WiFi.SSID(wifinumder).length(); i++)
    {
        lp.ssid[i] = WiFi.SSID(wifinumder)[i];
    }
    GP.BREAK();
    GP.PASS("ps", "Password", lp.pass);
    GP.SUBMIT("Submit");
    GP.FORM_END();
    GP.BUILD_END();
}

static void action(GyverPortal &p)
{
    if (p.form("/login")) // кнопка нажата
    {
        p.copyStr("lg", lp.ssid); // копируем себе
        p.copyStr("ps", lp.pass);
        EEPROM.put(0, lp);       // сохраняем
        EEPROM.commit();         // записываем
        WiFi.softAPdisconnect(); // отключаем AP
    }
}
