// L'email con la QSL attaccata: il messaggio che esce, e cosa succede quando
// manca qualcosa.
//
// Il dialogo con la casella vuole un server vero con un certificato buono —
// quello si prova mandandosi una QSL — ma il messaggio si costruisce qui, e
// li' dentro stanno le cose che si sbagliano: gli accenti nell'oggetto,
// l'allegato in base64, il punto a inizio riga che chiuderebbe il messaggio a
// meta'.
#include "core/MailSender.h"

#include <QSignalSpy>
#include <QTest>

using namespace decolog::core;

namespace {

MailAccount account()
{
    MailAccount a;
    a.host = QStringLiteral("smtp.gmail.com");
    a.port = 587;
    a.user = QStringLiteral("iu8lmc@gmail.com");
    a.password = QStringLiteral("una app password");
    a.fromName = QStringLiteral("Martino Merola");
    return a;
}

} // namespace

class TestMailSender : public QObject {
    Q_OBJECT

private slots:
    void theHeadersSayWhoAndWhat()
    {
        MailMessage m;
        m.to = QStringLiteral("dl9zzt@example.de");
        m.subject = QStringLiteral("QSL da IU8LMC");
        m.body = QStringLiteral("Grazie per il collegamento.");
        const QByteArray mime = buildMime(account(), m);

        QVERIFY(mime.contains("From: Martino Merola <iu8lmc@gmail.com>\r\n"));
        QVERIFY(mime.contains("To: dl9zzt@example.de\r\n"));
        QVERIFY(mime.contains("Subject: QSL da IU8LMC\r\n"));
        QVERIFY(mime.contains("MIME-Version: 1.0\r\n"));
        // Senza allegato non si tira su un multipart per niente.
        QVERIFY(mime.contains("Content-Type: text/plain; charset=UTF-8"));
        QVERIFY(!mime.contains("multipart/mixed"));
        QVERIFY(mime.contains("Grazie per il collegamento."));
    }

    void accentsInTheSubjectSurvive()
    {
        MailMessage m;
        m.to = QStringLiteral("f5def@example.fr");
        // Un oggetto con gli accenti in una intestazione non ci sta: va scritto
        // come dice la RFC 2047, se no arriva a pezzi.
        m.subject = QStringLiteral("La tua QSL è partita");
        const QByteArray mime = buildMime(account(), m);

        QVERIFY2(mime.contains("Subject: =?UTF-8?B?"), mime.left(300).constData());
        QVERIFY(!mime.contains("La tua QSL è partita"));
        // E si rilegge: quello che c'e' dentro e' proprio l'oggetto.
        const int start = mime.indexOf("Subject: =?UTF-8?B?") + 19;
        const int end = mime.indexOf("?=", start);
        QCOMPARE(QString::fromUtf8(QByteArray::fromBase64(mime.mid(start, end - start))),
                 QStringLiteral("La tua QSL è partita"));
    }

    void aLongSubjectWithASmileIsFolded()
    {
        // Un oggetto lungo con una faccina: pezzi da 75 caratteri al massimo,
        // come vuole la RFC 2047, e la faccina intera dentro un pezzo solo.
        MailMessage m;
        m.to = QStringLiteral("ik4idf@example.it");
        m.subject = QStringLiteral("Grazie per il bel QSO di stamattina in 20 metri, a presto 😊 73 de IU8LMC");
        const QByteArray mime = buildMime(account(), m);
        const int start = mime.indexOf("Subject: ");
        const int end = mime.indexOf("\r\nDate: ", start);
        const QByteArray header = mime.mid(start + 9, end - start - 9);
        QString decoded;
        for (QByteArray word : header.split('\n')) {
            word = word.trimmed();
            QVERIFY2(word.size() <= 75, word.constData());
            QVERIFY(word.startsWith("=?UTF-8?B?") && word.endsWith("?="));
            decoded += QString::fromUtf8(QByteArray::fromBase64(word.mid(10, word.size() - 12)));
        }
        QCOMPARE(decoded, m.subject);
    }

    void theCardTravelsAsAnAttachment()
    {
        MailMessage m;
        m.to = QStringLiteral("vk3abc@example.au");
        m.subject = QStringLiteral("QSL");
        m.body = QStringLiteral("La cartolina e' allegata.");
        m.attachmentName = QStringLiteral("VK3ABC-20260301-0912.png");
        // Un PNG finto ma lungo: quello che conta e' che venga spezzato bene.
        m.attachment = QByteArray(5000, '\x89');
        const QByteArray mime = buildMime(account(), m);

        QVERIFY(mime.contains("Content-Type: multipart/mixed; boundary=\"decodxlog-"));
        QVERIFY(mime.contains("Content-Type: image/png; name=\"VK3ABC-20260301-0912.png\""));
        QVERIFY(mime.contains("Content-Disposition: attachment; filename=\"VK3ABC-20260301-0912.png\""));
        QVERIFY(mime.contains("Content-Transfer-Encoding: base64"));

        // L'allegato si rilegge identico, e nessuna riga supera i 76 caratteri:
        // sopra quelli qualche server taglia.
        // L'allegato comincia dopo la riga vuota che chiude le sue intestazioni.
        const int headers = mime.indexOf("Content-Disposition: attachment");
        const int begin = mime.indexOf("\r\n\r\n", headers) + 4;
        const int stop = mime.indexOf("\r\n--decodxlog-", begin);
        QVERIFY(stop > begin);
        QByteArray encoded = mime.mid(begin, stop - begin);
        for (const QByteArray& line : encoded.split('\n'))
            QVERIFY2(line.trimmed().size() <= 76, QByteArray::number(line.size()).constData());
        encoded.replace("\r\n", "");
        QCOMPARE(QByteArray::fromBase64(encoded), m.attachment);
    }

    void aLineThatStartsWithADotDoesNotEndTheMessage()
    {
        MailMessage m;
        m.to = QStringLiteral("k1ab@example.com");
        m.subject = QStringLiteral("QSL");
        // Nel protocollo un punto da solo a inizio riga chiude il messaggio: se
        // non si raddoppia, tutto quello che viene dopo si perde per strada.
        m.body = QStringLiteral("Prima riga\n.\nUltima riga");
        const QByteArray mime = buildMime(account(), m);

        QVERIFY(mime.contains("Prima riga\r\n..\r\nUltima riga"));
        QVERIFY(mime.contains("Ultima riga"));
    }

    void withoutAMailboxItSaysSoInsteadOfTrying()
    {
        MailSender sender;
        QSignalSpy failures(&sender, &MailSender::failed);
        MailMessage m;
        m.to = QStringLiteral("dl9zzt@example.de");
        m.tag = 42;
        sender.send(m);   // nessun account impostato

        QCOMPARE(failures.size(), 1);
        QCOMPARE(failures.first().at(0).toLongLong(), 42LL);
        QVERIFY2(failures.first().at(2).toString().contains(QStringLiteral("mailbox")),
                 qPrintable(failures.first().at(2).toString()));
    }

    void withoutAnAddressItSaysWhichStation()
    {
        MailSender sender;
        sender.setAccount(account());
        QSignalSpy failures(&sender, &MailSender::failed);
        MailMessage m;
        m.tag = 7;   // niente destinatario: il callbook non aveva l'email
        sender.send(m);

        QCOMPARE(failures.size(), 1);
        QCOMPARE(failures.first().at(0).toLongLong(), 7LL);
        QVERIFY2(failures.first().at(2).toString().contains(QStringLiteral("email")),
                 qPrintable(failures.first().at(2).toString()));
    }
};

QTEST_MAIN(TestMailSender)
#include "tst_mailsender.moc"
