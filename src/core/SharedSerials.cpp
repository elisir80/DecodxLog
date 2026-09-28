#include "core/SharedSerials.h"

#include <QtGlobal>

namespace decolog::core {

void SharedSerials::note(int serial)
{
    if (serial > m_max)
        m_max = serial;
}

int SharedSerials::serve()
{
    const int n = qMax(m_next, m_max + 1);
    m_next = n + 1;
    note(n);
    return n;
}

bool SharedSerials::isServer(const QString& me, const QStringList& others)
{
    for (const QString& id : others) {
        if (id < me)
            return false;
    }
    return true;
}

int SharedSerials::take(int localNext, bool server, bool online)
{
    if (online && server)
        return qMax(serve(), localNext);
    int n = m_reserved > 0 ? m_reserved : qMax(localNext, m_max + 1);
    m_reserved = 0;
    note(n);
    return n;
}

void SharedSerials::reserve(int serial)
{
    note(serial);
    m_reserved = serial;
}

} // namespace decolog::core
