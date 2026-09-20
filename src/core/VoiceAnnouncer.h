// DecoDXLog — annunci vocali degli spot.
//
// Su Windows la sintesi del sistema (SAPI, le voci di Impostazioni → Voce), senza
// librerie in piu'; su macOS "say", su Linux spd-say o espeak-ng. Le frasi vanno in
// coda: due spot di fila non si parlano sopra.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace decolog::core {

class VoiceAnnouncer : public QObject {
    Q_OBJECT

public:
    explicit VoiceAnnouncer(QObject* parent = nullptr);
    ~VoiceAnnouncer() override;

    bool available() const;
    QString backend() const;
    // Nomi delle voci installate ("Microsoft Elsa Desktop - Italian").
    QStringList voices() const;
    void setVoice(const QString& name);
    // -10 (lento) .. 10 (veloce), 0 = normale.
    void setRate(int rate);
    // 0..100
    void setVolume(int volume);

    void say(const QString& text);
    void stop();

    // Un nominativo da leggere lettera per lettera, con l'alfabeto fonetico se
    // richiesto: "IU8LMC" → "I U 8 L M C" o "India Uniform 8 Lima Mike Charlie".
    static QString spell(const QString& callsign, bool phonetic);

private:
    struct Backend;
    std::unique_ptr<Backend> d;
};

} // namespace decolog::core
