#ifndef OTP_HPP
#define OTP_HPP

namespace esphome {
namespace otp {
    // public:

    uint32_t hotp_generate(uint8_t *key, size_t key_len, uint64_t interval, size_t digits);
    uint32_t totp_hash_token(uint8_t *key, size_t key_len, uint64_t time, size_t digits);
    uint32_t totp_generate(uint8_t *key, size_t key_len);

    int base32_encode(const uint8_t *data, int length, char *result, int encode_len);
    int base32_decode(const char *encoded, uint8_t *result, int buf_len);

    // private:

    void hotp_hmac(unsigned char *key, size_t ken_len, uint64_t interval, uint8_t *out);
    uint32_t hotp_dt(const uint8_t *digest);

} // namespace otp
} // namespace esphome

#endif // OTP_HPP