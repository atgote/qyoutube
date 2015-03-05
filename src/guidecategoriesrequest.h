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
 
#ifndef GUIDECATEGORIESREQUEST_H
#define GUIDECATEGORIESREQUEST_H

#include "request.h"

class QStringList;

namespace QYouTube {

class QYOUTUBESHARED_EXPORT GuideCategoriesRequest : public Request
{
    Q_OBJECT
    
public:
    explicit GuideCategoriesRequest(QObject *parent = 0);
    
public Q_SLOTS:
    void list(const QStringList &part, const QVariantMap &filters, const QVariantMap &params = QVariantMap());

private:
    Q_DISABLE_COPY(GuideCategoriesRequest)
};

}

#endif // GUIDECATEGORIESREQUEST_H
