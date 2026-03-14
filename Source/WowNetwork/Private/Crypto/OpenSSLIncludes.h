#pragma once

// Fix conflict between UE's "namespace UI" (ObjectMacros.h) and
// OpenSSL's "typedef struct ui_st UI" (ossl_typ.h).
// We rename OpenSSL's UI to OPENSSL_UI to avoid the collision.

#define UI OPENSSL_UI

THIRD_PARTY_INCLUDES_START
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
THIRD_PARTY_INCLUDES_END

#undef UI

// Restore UE's UI namespace (it will be available from subsequent includes)
