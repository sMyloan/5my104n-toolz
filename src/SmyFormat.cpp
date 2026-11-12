#include "SmyFormat.h"
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <functional>
#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <cstring>
#include <stdexcept>
#include <ctime>

static const QByteArray MAGIC = QByteArray("SMY\x00", 4);
static const quint8 VERSION = 0x01;
//соль для pbkdf2 должна быть одинаковой для всех модулей чтобы ключи воспроизводились
static const QByteArray SMY_SALT = "5MY104N_SMY_V1";

static QByteArray zlibCompress(const QByteArray& in) {
    uLongf outLen = compressBound(in.size());
    QByteArray out(outLen, 0);
    if (::compress2((Bytef*)out.data(), &outLen,
        (const Bytef*)in.constData(), in.size(), 9) != Z_OK)
        throw std::runtime_error("zlib compress failed");
    out.resize(outLen);
    return out;
}

static QByteArray zlibDecompress(const QByteArray& in) {
    uLongf outLen = in.size() * 4;
    QByteArray out(outLen, 0);
    int rc;
    //повторное увеличение буфера при недостаточном размере
    while ((rc = ::uncompress((Bytef*)out.data(), &outLen,
        (const Bytef*)in.constData(), in.size())) == Z_BUF_ERROR) {
        outLen *= 2;
    out.resize(outLen);
        }
        if (rc != Z_OK) throw std::runtime_error("zlib decompress failed");
        out.resize(outLen);
    return out;
}

//urlsafe base64 для совместимости с fernet форматом python
static QByteArray urlsafeB64Encode(const QByteArray& in) {
    QByteArray b = in.toBase64();
    b.replace('+', '-').replace('/', '_');
    while (b.endsWith('=')) b.chop(1);
    return b;
}

static QByteArray urlsafeB64Decode(const QByteArray& in) {
    QByteArray b = in;
    b.replace('-', '+').replace('_', '/');
    while (b.size() % 4) b.append('=');
    return QByteArray::fromBase64(b);
}

//derive fernet ключа через pbkdf2 с 200000 итерациями
static QByteArray deriveFernetKey(const QString& password) {
    QByteArray out(32, 0);
    if (!PKCS5_PBKDF2_HMAC(password.toUtf8().constData(), password.size(),
        (const unsigned char*)SMY_SALT.constData(), SMY_SALT.size(),
                           200000, EVP_sha256(), 32, (unsigned char*)out.data()))
        throw std::runtime_error("PBKDF2 failed");
    return urlsafeB64Encode(out);
}

//шифрование aes-128-cbc с hmac-sha256 по спецификации fernet
static QByteArray fernetEncrypt(const QByteArray& data, const QByteArray& fernetKeyB64) {
    QByteArray keyBin = urlsafeB64Decode(fernetKeyB64);
    if (keyBin.size() != 32) throw std::runtime_error("Bad Fernet key size");
    QByteArray signingKey = keyBin.left(16);
    QByteArray encKey = keyBin.mid(16, 16);
    QByteArray iv(16, 0);
    if (!RAND_bytes((unsigned char*)iv.data(), 16))
        throw std::runtime_error("RAND_bytes failed");
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
        (const unsigned char*)encKey.constData(),
                           (const unsigned char*)iv.constData()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("EncryptInit failed");
                           }
                           QByteArray ciphertext(data.size() + 16, 0);
                           int outLen = 0, totalLen = 0;
                           if (EVP_EncryptUpdate(ctx, (unsigned char*)ciphertext.data(), &outLen,
                               (const unsigned char*)data.constData(), data.size()) != 1) {
                               EVP_CIPHER_CTX_free(ctx);
                           throw std::runtime_error("EncryptUpdate failed");
                               }
                               totalLen = outLen;
                               if (EVP_EncryptFinal_ex(ctx, (unsigned char*)ciphertext.data() + totalLen, &outLen) != 1) {
                                   EVP_CIPHER_CTX_free(ctx);
                                   throw std::runtime_error("EncryptFinal failed");
                               }
                               totalLen += outLen;
                               ciphertext.resize(totalLen);
                               EVP_CIPHER_CTX_free(ctx);
                               //версия fernet токена фиксирована как 0x80
                               quint8 version = 0x80;
                               quint64 t = (quint64)std::time(nullptr);
                               QByteArray timeBytes(8, 0);
                               for (int i = 7; i >= 0; --i) { timeBytes[i] = (char)(t & 0xFF); t >>= 8; }
                               QByteArray toSign;
                               toSign.reserve(1 + 8 + iv.size() + ciphertext.size());
                               toSign.append((char)version);
                               toSign.append(timeBytes);
                               toSign.append(iv);
                               toSign.append(ciphertext);
                               //hmac подписывает все кроме себя для защиты от подделки
                               QByteArray hmac(32, 0);
                               unsigned int hmacLen = 0;
                               HMAC(EVP_sha256(), signingKey.constData(), signingKey.size(),
                                    (const unsigned char*)toSign.constData(), toSign.size(),
                                    (unsigned char*)hmac.data(), &hmacLen);
                               hmac.resize(hmacLen);
                               QByteArray token;
                               token.append((char)version);
                               token.append(timeBytes);
                               token.append(iv);
                               token.append(ciphertext);
                               token.append(hmac);
                               return urlsafeB64Encode(token);
}

static QByteArray fernetDecrypt(const QByteArray& tokenB64, const QByteArray& fernetKeyB64) {
    QByteArray keyBin = urlsafeB64Decode(fernetKeyB64);
    if (keyBin.size() != 32) throw std::runtime_error("Bad Fernet key size");
    QByteArray signingKey = keyBin.left(16);
    QByteArray encKey = keyBin.mid(16, 16);
    QByteArray token = urlsafeB64Decode(tokenB64);
    if (token.size() < 57) throw std::runtime_error("Token too short");
    if ((quint8)token[0] != 0x80) throw std::runtime_error("Bad Fernet version");
    QByteArray hmacPart = token.right(32);
    QByteArray toSign = token.left(token.size() - 32);
    QByteArray expected(32, 0);
    unsigned int hmacLen = 0;
    HMAC(EVP_sha256(), signingKey.constData(), signingKey.size(),
         (const unsigned char*)toSign.constData(), toSign.size(),
         (unsigned char*)expected.data(), &hmacLen);
    expected.resize(hmacLen);
    //константное сравнение hmac для защиты от timing атак
    bool ok = (expected.size() == hmacPart.size());
    for (int i = 0; i < expected.size(); ++i)
        ok &= (expected[i] == hmacPart[i]);
    if (!ok) throw std::runtime_error("Wrong password or corrupted file");
    QByteArray iv = token.mid(9, 16);
    QByteArray ciphertext = token.mid(25, token.size() - 25 - 32);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
        (const unsigned char*)encKey.constData(),
                           (const unsigned char*)iv.constData()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("DecryptInit failed");
                           }
                           QByteArray plaintext(ciphertext.size() + 16, 0);
                           int outLen = 0, totalLen = 0;
                           if (EVP_DecryptUpdate(ctx, (unsigned char*)plaintext.data(), &outLen,
                               (const unsigned char*)ciphertext.constData(), ciphertext.size()) != 1) {
                               EVP_CIPHER_CTX_free(ctx);
                           throw std::runtime_error("DecryptUpdate failed");
                               }
                               totalLen = outLen;
                               if (EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext.data() + totalLen, &outLen) != 1) {
                                   EVP_CIPHER_CTX_free(ctx);
                                   throw std::runtime_error("DecryptFinal failed (bad padding / wrong key)");
                               }
                               totalLen += outLen;
                               plaintext.resize(totalLen);
                               EVP_CIPHER_CTX_free(ctx);
                               return plaintext;
}

void SmyFormat::pack(const QString& scriptPath, const QJsonObject& manifest,
                     const QString& outPath, const QString& password) {
    QFile sf(scriptPath);
    if (!sf.open(QIODevice::ReadOnly))
        throw std::runtime_error(("Script not found: " + scriptPath).toStdString());
    QByteArray rawBody = sf.readAll();
    sf.close();
    QJsonObject m = manifest;
    m["magic"] = "SMY";
    m["version"] = "1.0";
    //заголовок json завершается нулем для четкого отделения от тела
    QByteArray headerJson = QJsonDocument(m).toJson(QJsonDocument::Indented) + '\0';
    QByteArray compressed = zlibCompress(rawBody);
    if (!password.isEmpty()) {
        QByteArray key = deriveFernetKey(password);
        compressed = fernetEncrypt(compressed, key);
    }
    QFile of(outPath);
    if (!of.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error("Cannot write output");
    of.write(MAGIC);
    of.write((char*)&VERSION, 1);
    quint32 hlen = headerJson.size();
    of.write((char*)&hlen, 4);
    of.write(headerJson);
    quint32 blen = compressed.size();
    of.write((char*)&blen, 4);
    of.write(compressed);
                     }
                     
                     QPair<QJsonObject, QByteArray> SmyFormat::unpack(const QString& path, const QString& password) {
                         QFile f(path);
                         if (!f.open(QIODevice::ReadOnly))
                             throw std::runtime_error("Cannot open file");
                         QByteArray data = f.readAll();
                         if (data.size() < 9 || data.left(4) != MAGIC)
                             throw std::runtime_error("Not an SMY file (bad magic)");
                         int pos = 4;
                         quint8 ver = (quint8)data[pos++];
                         if (ver != VERSION)
                             throw std::runtime_error(("Unsupported format version: " + std::to_string(ver)).c_str());
                         quint32 hlen; memcpy(&hlen, data.constData() + pos, 4); pos += 4;
                         if ((quint32)data.size() < pos + hlen + 4)
                             throw std::runtime_error("Truncated SMY file");
                         QByteArray headerRaw = data.mid(pos, hlen);
                         pos += hlen;
                         if (headerRaw.endsWith('\0')) headerRaw.chop(1);
                         QJsonObject manifest = QJsonDocument::fromJson(headerRaw).object();
                         quint32 blen; memcpy(&blen, data.constData() + pos, 4); pos += 4;
                         QByteArray body = data.mid(pos, blen);
                         if (!password.isEmpty()) {
                             QByteArray key = deriveFernetKey(password);
                             body = fernetDecrypt(body, key);
                         }
                         QByteArray script = zlibDecompress(body);
                         return {manifest, script};
                     }
                     
                     QString SmyFormat::extract(const QString& smyPath, const QString& workdir, const QString& password) {
                         auto [manifest, scriptBytes] = unpack(smyPath, password);
                         QDir().mkpath(workdir);
                         QString entry = manifest.value("entry").toString("script.py");
                         QString outPath = QDir(workdir).filePath(entry);
                         QFile f(outPath);
                         if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                             throw std::runtime_error("Cannot write extracted");
                         f.write(scriptBytes);
                         f.close();
                         //исполняемые права только для владельца для безопасности
                         QFile::setPermissions(outPath, QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
                         return outPath;
                     }
                     
                     QJsonObject SmyFormat::createDefaultManifest(const QString& name, const QString& description,
                                                                  const QString& entry, const QString& category,
                                                                  const QStringList& apt, const QStringList& pip,
                                                                  const QStringList& binary, const QString& author) {
                         QJsonObject m;
                         m["magic"] = "SMY";
                         m["version"] = "1.0";
                         m["name"] = name;
                         m["description"] = description;
                         m["author"] = author;
                         m["category"] = category;
                         m["entry"] = entry;
                         m["runtime"] = "python3";
                         m["requires_root"] = true;
                         QJsonArray a, p, b, t;
                         for (const QString& s : apt) a.append(s);
                         for (const QString& s : pip) p.append(s);
                         for (const QString& s : binary) b.append(s);
                         QJsonObject deps;
                         deps["apt"] = a; deps["pip"] = p; deps["binary"] = b;
                         m["dependencies"] = deps;
                         m["tags"] = t;
                         m["args"] = QJsonArray();
                         m["icon"] = QJsonValue::Null;
                         return m;
                                                                  }
                                                                  
                                                                  //кеш проверок зависимостей чтобы не дергать dpkg/pip при каждом клике
                                                                  static QMutex g_depCacheMutex;
                                                                  struct DepCacheEntry { qint64 timestampMs = 0; bool installed = false; };
                                                                  static QHash<QString, DepCacheEntry> g_depCache;
                                                                  //время жизни кеша 60 секунд
                                                                  static constexpr qint64 DEP_CACHE_TTL_MS = 60 * 1000;
                                                                  
                                                                  static bool cachedDependencyCheck(const QString& key, const std::function<bool()>& probe) {
                                                                      const qint64 now = QDateTime::currentMSecsSinceEpoch();
                                                                      {
                                                                          QMutexLocker lock(&g_depCacheMutex);
                                                                          auto it = g_depCache.constFind(key);
                                                                          if (it != g_depCache.constEnd() && now - it->timestampMs < DEP_CACHE_TTL_MS)
                                                                              return it->installed;
                                                                      }
                                                                      const bool installed = probe();
                                                                      {
                                                                          QMutexLocker lock(&g_depCacheMutex);
                                                                          g_depCache.insert(key, {now, installed});
                                                                      }
                                                                      return installed;
                                                                  }
                                                                  
                                                                  static bool pkgInstalledApt(const QString& pkg) {
                                                                      return cachedDependencyCheck("apt:" + pkg, [&pkg]() {
                                                                          QProcess p;
                                                                          p.start("dpkg", QStringList{"-s", pkg});
                                                                          if (!p.waitForFinished(1500)) {
                                                                              p.kill();
                                                                              p.waitForFinished(1000);
                                                                              return false;
                                                                          }
                                                                          return p.exitCode() == 0;
                                                                      });
                                                                  }
                                                                  
                                                                  static bool pkgInstalledPip(const QString& pkg) {
                                                                      return cachedDependencyCheck("pip:" + pkg, [&pkg]() {
                                                                          QProcess p;
                                                                          p.start("pip3", QStringList{"show", pkg});
                                                                          if (!p.waitForFinished(2000)) {
                                                                              p.kill();
                                                                              p.waitForFinished(1000);
                                                                              return false;
                                                                          }
                                                                          return p.exitCode() == 0;
                                                                      });
                                                                  }
                                                                  
                                                                  static bool binaryExists(const QString& bin) {
                                                                      return cachedDependencyCheck("bin:" + bin, [&bin]() {
                                                                          QProcess p;
                                                                          p.start("which", QStringList{bin});
                                                                          if (!p.waitForFinished(1000)) {
                                                                              p.kill();
                                                                              p.waitForFinished(1000);
                                                                              return false;
                                                                          }
                                                                          return p.exitCode() == 0;
                                                                      });
                                                                  }
                                                                  
                                                                  QMap<QString, QStringList> SmyFormat::checkDependencies(const QJsonObject& manifest) {
                                                                      QMap<QString, QStringList> missing;
                                                                      QJsonObject deps = manifest.value("dependencies").toObject();
                                                                      for (const auto& v : deps.value("apt").toArray()) {
                                                                          QString pkg = v.toString();
                                                                          if (!pkgInstalledApt(pkg)) missing["apt"].append(pkg);
                                                                      }
                                                                      for (const auto& v : deps.value("pip").toArray()) {
                                                                          QString pkg = v.toString();
                                                                          if (!pkgInstalledPip(pkg)) missing["pip"].append(pkg);
                                                                      }
                                                                      for (const auto& v : deps.value("binary").toArray()) {
                                                                          QString bin = v.toString();
                                                                          if (!binaryExists(bin)) missing["binary"].append(bin);
                                                                      }
                                                                      return missing;
                                                                  }
                                                                  
                                                                  bool SmyFormat::installDependencies(const QMap<QString, QStringList>& missing) {
                                                                      bool ok = true;
                                                                      if (missing.contains("apt") && !missing["apt"].isEmpty()) {
                                                                          QProcess::execute("apt-get", QStringList{"update"});
                                                                          int rc = QProcess::execute("apt-get", QStringList{"install", "-y"} + missing["apt"]);
                                                                          if (rc != 0) ok = false;
                                                                      }
                                                                      if (missing.contains("pip") && !missing["pip"].isEmpty()) {
                                                                          int rc = QProcess::execute("pip3", QStringList{"install"} + missing["pip"]);
                                                                          if (rc != 0) ok = false;
                                                                      }
                                                                      if (missing.contains("binary") && !missing["binary"].isEmpty()) ok = false;
                                                                      //сброс кеша после установки чтобы следующие проверки увидели новые пакеты
                                                                      {
                                                                          QMutexLocker lock(&g_depCacheMutex);
                                                                          g_depCache.clear();
                                                                      }
                                                                      return ok;
                                                                  }
