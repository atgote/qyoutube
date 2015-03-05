/*
 * Copyright (C) 2015 Stuart Howarth <showarth@marxoft.co.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "authenticationrequest.h"
#include "request_p.h"
#include "urls.h"
#include <QNetworkReply>
#include <QStringList>
#include <QTimer>
#include <QTime>

namespace QYouTube {

class AuthenticationRequestPrivate : public RequestPrivate
{

public:
    enum AuthRequest {
        WebToken = 0,
        DeviceToken,
        DeviceCode,
        RevokeToken
    };
    
    AuthenticationRequestPrivate(AuthenticationRequest *parent) :
        RequestPrivate(parent),
        authRequest(WebToken),
        deviceExpiry(0),
        deviceInterval(5000)
    {
    }
    
    void _q_pollForDeviceToken() {
        Q_Q(AuthenticationRequest);
        
        authRequest = DeviceToken;
        q->setUrl(QUrl(TOKEN_URL));
        q->setData(QString("client_id=" + clientId + "&client_secret=" + clientSecret + "&code=" + deviceCode +
                           "&grant_type=http://oauth.net/grant_type/device/1.0"));
        q->post();
    }
    
    void _q_onReplyFinished() {
        if (!reply) {
            return;
        }
    
        Q_Q(AuthenticationRequest);
    
        bool ok;
        setResult(QtJson::Json::parse(reply->readAll(), ok));
    
        switch (reply->error()) {
        case QNetworkReply::NoError:
            break;
        case QNetworkReply::OperationCanceledError:
            setStatus(Request::Canceled);
            setError(Request::NoError);
            setErrorString(QString());
            emit q->finished();
            return;
        default:
            setStatus(Request::Failed);
            setError(Request::Error(reply->error()));
            setErrorString(reply->errorString());
            emit q->finished();
            return;
        }
    
        if (ok) {
            QVariantMap map = result.toMap();
            
            switch (authRequest) {
            case WebToken:
                setStatus(Request::Ready);
                setError(Request::NoError);
                setErrorString(QString());
                break;
            case DeviceToken:
                if (map.value("error").isNull()) {
                    setStatus(Request::Ready);
                    setError(Request::NoError);
                    setErrorString(QString());
                }
                else {
                    QString es = map.value("error").toString();
                
                    if (deviceExpiryTime.elapsed() < deviceExpiry) {
                        if (es == "authorization_pending") {
                            QTimer::singleShot(deviceInterval, q, SLOT(_q_pollForDeviceToken()));
                            return;
                        }
                
                        if (es == "slow_down") {
                            deviceInterval *= 2;
                            QTimer::singleShot(deviceInterval, q, SLOT(_q_pollForDeviceToken()));
                            return;
                        }
                    }
                
                    setStatus(Request::Failed);
                    setError(Request::UnknownContentError);
                    setErrorString(es);
                }
                
                break;
            case DeviceCode:
                if (!map.value("device_code").isNull()) {
                    int expiry = map.value("expires_in").toInt();
                    int interval = map.value("interval").toInt();
                    deviceCode = map.value("device_code").toString();
                    deviceExpiry = (expiry > 0 ? expiry * 1000 : 60000);
                    deviceInterval = (interval > 0 ? interval * 1100 : 5000);
                    deviceExpiryTime.start();
                    QTimer::singleShot(deviceInterval, q, SLOT(_q_pollForDeviceToken()));
                    emit q->authorizationCodeReady(map);
                    return;
                }
                
                break;
            default:
                break;
            }
        }
        else if (authRequest == RevokeToken) {
            setStatus(Request::Ready);
            setError(Request::NoError);
            setErrorString(QString());
        }
        else {
            setStatus(Request::Failed);
            setError(Request::ParseError);
            setErrorString(Request::tr("Unable to parse response"));
        }
    
        emit q->finished();
    }
    
    AuthRequest authRequest;
    
    int deviceExpiry;
    int deviceInterval;
    
    QTime deviceExpiryTime;
    
    QString deviceCode;
    
    Q_DECLARE_PUBLIC(AuthenticationRequest)
};

/*!
    \class AuthenticationRequest
    \brief Handles OAuth 2.0 authentication requests.
    
    \ingroup authentication
    \ingroup requests
    
    The AuthenticationRequest class is used for obtaining and revoking access tokens for use with the YouTube Data API. 
    AuthenticationRequest supports the <a target="_blank" 
    href="https://developers.google.com/youtube/v3/guides/authentication#installed-apps">installed applications 
    flow</a> and the <a target="_blank" 
    href="https://developers.google.com/youtube/v3/guides/authentication#devices">devices flow</a>.
*/
AuthenticationRequest::AuthenticationRequest(QObject *parent) :
    Request(*new AuthenticationRequestPrivate(this), parent)
{
}

/*!
    \brief Submits code in exchange for a YouTube access token.
    
    This method is used when authenticating via a web view using the <a target="_blank" 
    href="https://developers.google.com/youtube/v3/guides/authentication#installed-apps">installed applications 
    flow</a>. The code is retrieved from the title of the web page.
*/
void AuthenticationRequest::exchangeCodeForAccessToken(const QString &code) {
    Q_D(AuthenticationRequest);
    d->authRequest = AuthenticationRequestPrivate::WebToken;
    setUrl(QUrl(TOKEN_URL));
    setData(QString("code=" + code + "&client_id=" + clientId() + "&client_secret=" + clientSecret() +
                    "&redirect_uri=urn:ietf:wg:oauth:2.0:oob&grant_type=authorization_code"));
    post();
}

/*!
    \fn void AuthenticationRequest::authorizationCodeReady(const QVariantMap &code)
    \brief Emitted when an authorization code has been obtained using requestAuthorizationCode().
*/

/*!
    \brief Requests a device authorization code for the specified scopes.
    
    This method is used when using the <a target="_blank" 
    href="https://developers.google.com/youtube/v3/guides/authentication#devices">devices flow</a>.
    
    After obtaining the authorization code, AuthenticationRequest will emit the authorizationCodeReady() signal and 
    begin polling the server at regular intervals until either the user has granted permission or the authorization code 
    has expired. In either case the finished() signal will be emitted, and the result will either contain a valid 
    access token, or information about any error.
    
    The YouTube Data API supports the following scopes:
    
    <table>
        <tr>
            <th>Scope</th>
            <th>Description</th>
        </tr>
        <tr>
            <td>https://www.googleapis.com/auth/youtube</td>
            <td>Manage a YouTube account.</td>
        </tr>
        <tr>
            <td>https://www.googleapis.com/auth/youtube.readonly</td>
            <td>View a YouTube account.</td>
        </tr>
        <tr>
            <td>https://www.googleapis.com/auth/youtube.upload</td>
            <td>Upload YouTube videos and manage YouTube videos.</td>
        </tr>
        <tr>
            <td>https://www.googleapis.com/auth/youtubepartner-channel-audit</td>
            <td>Retrieve the auditDetails part in a channel resource.</td>
        </tr>
    </table>
*/
void AuthenticationRequest::requestAuthorizationCode(const QStringList &scopes) {
    Q_D(AuthenticationRequest);
    d->authRequest = AuthenticationRequestPrivate::DeviceCode;
    setUrl(QUrl(DEVICE_CODE_URL));
    setData(QString("scope=" + scopes.join(" ") + "&client_id=" + clientId()));
    post();
}

/*!
    \brief Revokes YouTube Data API access for the specified access token.
*/
void AuthenticationRequest::revokeAccessToken(const QString &token) {
    Q_D(AuthenticationRequest);
    d->authRequest = AuthenticationRequestPrivate::RevokeToken;
    QUrl u(REVOKE_TOKEN_URL);
#if QT_VERSION >= 0x050000
    QUrlQuery query(u);
    query.addQueryItem("token", token);
    u.setQuery(query);
#else
    u.addQueryItem("token", token);
#endif
    setUrl(u);
    get(false);
}

}

#include "moc_authenticationrequest.cpp"