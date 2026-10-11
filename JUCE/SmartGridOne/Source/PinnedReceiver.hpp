#pragma once

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <CommonCrypto/CommonDigest.h>
#include <string>

// The Bonjour fingerprint is paired with the access code. Both channels use
// the same leaf-certificate check and refuse redirects.
//
inline bool ReceiverCertificateMatches(SecTrustRef trust, const std::string& fingerprint)
{
    SecCertificateRef certificate = trust == nullptr || SecTrustGetCertificateCount(trust) == 0
        ? nullptr : SecTrustGetCertificateAtIndex(trust, 0);
    CFDataRef der = certificate == nullptr ? nullptr : SecCertificateCopyData(certificate);
    if (der == nullptr)
    {
        return false;
    }

    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(CFDataGetBytePtr(der), static_cast<CC_LONG>(CFDataGetLength(der)), digest);
    CFRelease(der);
    constexpr char x_digits[] = "0123456789abcdef";
    std::string hex;
    for (const auto byte : digest)
    {
        hex += x_digits[byte >> 4];
        hex += x_digits[byte & 15];
    }

    return hex == fingerprint;
}
