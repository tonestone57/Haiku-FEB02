/*
 * Copyright 2012, Michael Lotz, mmlr@mlotz.ch. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */


#include "Keyring.h"

#include <OS.h>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/rand.h>

#ifndef OSSL_KDF_PARAM_LANES
#ifdef OSSL_KDF_PARAM_ARGON2_LANES
#define OSSL_KDF_PARAM_LANES OSSL_KDF_PARAM_ARGON2_LANES
#else
#define OSSL_KDF_PARAM_LANES "lanes"
#endif
#endif

#ifndef OSSL_KDF_PARAM_MEMCOST
#ifdef OSSL_KDF_PARAM_ARGON2_MEMCOST
#define OSSL_KDF_PARAM_MEMCOST OSSL_KDF_PARAM_ARGON2_MEMCOST
#else
#define OSSL_KDF_PARAM_MEMCOST "memcost"
#endif
#endif
#endif


Keyring::Keyring()
	:
	fHasUnlockKey(false),
	fUnlocked(false),
	fModified(false)
{
}


Keyring::Keyring(const char* name)
	:
	fName(name),
	fHasUnlockKey(false),
	fUnlocked(false),
	fModified(false)
{
}


Keyring::~Keyring()
{
}


status_t
Keyring::ReadFromMessage(const BMessage& message)
{
	status_t result = message.FindString("name", &fName);
	if (result != B_OK)
		return result;

	result = message.FindBool("hasUnlockKey", &fHasUnlockKey);
	if (result != B_OK)
		return result;

	if (message.GetBool("noData", false)) {
		fFlatBuffer.SetSize(0);
		return B_OK;
	}

	ssize_t size;
	const void* data;
	result = message.FindData("data", B_RAW_TYPE, &data, &size);
	if (result != B_OK)
		return result;

	if (size < 0)
		return B_ERROR;

	fFlatBuffer.SetSize(0);
	ssize_t written = fFlatBuffer.WriteAt(0, data, size);
	if (written != size) {
		fFlatBuffer.SetSize(0);
		return written < 0 ? written : B_ERROR;
	}

	return B_OK;
}


status_t
Keyring::WriteToMessage(BMessage& message)
{
	status_t result = _EncryptToFlatBuffer();
	if (result != B_OK)
		return result;

	if (fFlatBuffer.BufferLength() == 0)
		result = message.AddBool("noData", true);
	else {
		result = message.AddData("data", B_RAW_TYPE, fFlatBuffer.Buffer(),
			fFlatBuffer.BufferLength());
	}
	if (result != B_OK)
		return result;

	result = message.AddBool("hasUnlockKey", fHasUnlockKey);
	if (result != B_OK)
		return result;

	return message.AddString("name", fName);
}


status_t
Keyring::Unlock(const BMessage* keyMessage)
{
	if (fUnlocked)
		return B_OK;

	if (fHasUnlockKey == (keyMessage == NULL))
		return B_BAD_VALUE;

	if (keyMessage != NULL)
		fUnlockKey = *keyMessage;

	status_t result = _DecryptFromFlatBuffer();
	if (result != B_OK) {
		fUnlockKey.MakeEmpty();
		return result;
	}

	fUnlocked = true;
	return B_OK;
}


void
Keyring::Lock()
{
	if (!fUnlocked)
		return;

	_EncryptToFlatBuffer();

	fUnlockKey.MakeEmpty();
	fData.MakeEmpty();
	fApplications.MakeEmpty();
	fUnlocked = false;
}


bool
Keyring::IsUnlocked() const
{
	return fUnlocked;
}


bool
Keyring::HasUnlockKey() const
{
	return fHasUnlockKey;
}


const BMessage&
Keyring::UnlockKey() const
{
	return fUnlockKey;
}


status_t
Keyring::SetUnlockKey(const BMessage& keyMessage)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	fHasUnlockKey = true;
	fUnlockKey = keyMessage;
	fModified = true;
	return B_OK;
}


status_t
Keyring::RemoveUnlockKey()
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	fUnlockKey.MakeEmpty();
	fHasUnlockKey = false;
	fModified = true;
	return B_OK;
}


status_t
Keyring::GetNextApplication(uint32& cookie, BString& signature,
	BString& path)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	char* nameFound = NULL;
	status_t result = fApplications.GetInfo(B_MESSAGE_TYPE, cookie++,
		&nameFound, NULL);
	if (result != B_OK)
		return B_ENTRY_NOT_FOUND;

	BMessage appMessage;
	result = fApplications.FindMessage(nameFound, &appMessage);
	if (result != B_OK)
		return B_ENTRY_NOT_FOUND;

	result = appMessage.FindString("path", &path);
	if (result != B_OK)
		return B_ERROR;

	signature = nameFound;
	return B_OK;
}


status_t
Keyring::FindApplication(const char* signature, const char* path,
	BMessage& appMessage)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	int32 count;
	type_code type;
	if (fApplications.GetInfo(signature, &type, &count) != B_OK)
		return B_ENTRY_NOT_FOUND;

	for (int32 i = 0; i < count; i++) {
		if (fApplications.FindMessage(signature, i, &appMessage) != B_OK)
			continue;

		BString appPath;
		if (appMessage.FindString("path", &appPath) != B_OK)
			continue;

		if (appPath == path)
			return B_OK;
	}

	appMessage.MakeEmpty();
	return B_ENTRY_NOT_FOUND;
}


status_t
Keyring::AddApplication(const char* signature, const BMessage& appMessage)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	status_t result = fApplications.AddMessage(signature, &appMessage);
	if (result != B_OK)
		return result;

	fModified = true;
	return B_OK;
}


status_t
Keyring::RemoveApplication(const char* signature, const char* path)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	if (path == NULL) {
		// We want all of the entries for this signature removed.
		status_t result = fApplications.RemoveName(signature);
		if (result != B_OK)
			return B_ENTRY_NOT_FOUND;

		fModified = true;
		return B_OK;
	}

	int32 count;
	type_code type;
	if (fApplications.GetInfo(signature, &type, &count) != B_OK)
		return B_ENTRY_NOT_FOUND;

	for (int32 i = 0; i < count; i++) {
		BMessage appMessage;
		if (fApplications.FindMessage(signature, i, &appMessage) != B_OK)
			return B_ERROR;

		BString appPath;
		if (appMessage.FindString("path", &appPath) != B_OK)
			continue;

		if (appPath == path) {
			fApplications.RemoveData(signature, i);
			fModified = true;
			return B_OK;
		}
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
Keyring::FindKey(const BString& identifier, const BString& secondaryIdentifier,
	bool secondaryIdentifierOptional, BMessage* _foundKeyMessage) const
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	int32 count;
	type_code type;
	if (fData.GetInfo(identifier, &type, &count) != B_OK)
		return B_ENTRY_NOT_FOUND;

	// We have a matching primary identifier, need to check for the secondary
	// identifier.
	for (int32 i = 0; i < count; i++) {
		BMessage candidate;
		if (fData.FindMessage(identifier, i, &candidate) != B_OK)
			return B_ERROR;

		BString candidateIdentifier;
		if (candidate.FindString("secondaryIdentifier",
				&candidateIdentifier) != B_OK) {
			candidateIdentifier = "";
		}

		if (candidateIdentifier == secondaryIdentifier) {
			if (_foundKeyMessage != NULL)
				*_foundKeyMessage = candidate;
			return B_OK;
		}
	}

	// We didn't find an exact match.
	if (secondaryIdentifierOptional) {
		if (_foundKeyMessage == NULL)
			return B_OK;

		// The secondary identifier is optional, so we just return the
		// first entry.
		return fData.FindMessage(identifier, 0, _foundKeyMessage);
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
Keyring::FindKey(BKeyType type, BKeyPurpose purpose, uint32 index,
	BMessage& _foundKeyMessage) const
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	for (int32 keyIndex = 0;; keyIndex++) {
		int32 count = 0;
		char* identifier = NULL;
		if (fData.GetInfo(B_MESSAGE_TYPE, keyIndex, &identifier, NULL,
				&count) != B_OK) {
			break;
		}

		if (type == B_KEY_TYPE_ANY && purpose == B_KEY_PURPOSE_ANY) {
			// No need to inspect the actual keys.
			if ((int32)index >= count) {
				index -= count;
				continue;
			}

			return fData.FindMessage(identifier, index, &_foundKeyMessage);
		}

		// Go through the keys to check their type and purpose.
		for (int32 subkeyIndex = 0; subkeyIndex < count; subkeyIndex++) {
			BMessage subkey;
			if (fData.FindMessage(identifier, subkeyIndex, &subkey) != B_OK)
				return B_ERROR;

			bool match = true;
			if (type != B_KEY_TYPE_ANY) {
				BKeyType subkeyType;
				if (subkey.FindUInt32("type", (uint32*)&subkeyType) != B_OK)
					return B_ERROR;

				match = subkeyType == type;
			}

			if (match && purpose != B_KEY_PURPOSE_ANY) {
				BKeyPurpose subkeyPurpose;
				if (subkey.FindUInt32("purpose", (uint32*)&subkeyPurpose)
						!= B_OK) {
					return B_ERROR;
				}

				match = subkeyPurpose == purpose;
			}

			if (match) {
				if (index == 0) {
					_foundKeyMessage = subkey;
					return B_OK;
				}

				index--;
			}
		}
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
Keyring::AddKey(const BString& identifier, const BString& secondaryIdentifier,
	const BMessage& keyMessage)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	// Check for collisions.
	if (FindKey(identifier, secondaryIdentifier, false, NULL) == B_OK)
		return B_NAME_IN_USE;

	// We're fine, just add the new key.
	status_t result = fData.AddMessage(identifier, &keyMessage);
	if (result != B_OK)
		return result;

	fModified = true;
	return B_OK;
}


status_t
Keyring::RemoveKey(const BString& identifier,
	const BMessage& keyMessage)
{
	if (!fUnlocked)
		return B_NOT_ALLOWED;

	int32 count;
	type_code type;
	if (fData.GetInfo(identifier, &type, &count) != B_OK)
		return B_ENTRY_NOT_FOUND;

	for (int32 i = 0; i < count; i++) {
		BMessage candidate;
		if (fData.FindMessage(identifier, i, &candidate) != B_OK)
			return B_ERROR;

		// We require an exact match.
		if (!candidate.HasSameData(keyMessage))
			continue;

		status_t result = fData.RemoveData(identifier, i);
		if (result != B_OK)
			return result;

		fModified = true;
		return B_OK;
	}

	return B_ENTRY_NOT_FOUND;
}


int
Keyring::Compare(const Keyring* one, const Keyring* two)
{
	return strcmp(one->Name(), two->Name());
}


int
Keyring::Compare(const BString* name, const Keyring* keyring)
{
	return strcmp(name->String(), keyring->Name());
}


#ifdef HAVE_OPENSSL
static bool
HasHardwareAES()
{
#if defined(__i386__) || defined(__x86_64__)
	cpuid_info info;
	if (get_cpuid(&info, 1, 0) == B_OK) {
		// AES-NI is bit 25 of ECX (extended_features)
		if ((info.eax_1.extended_features & (1 << 25)) != 0)
			return true;
	}
#endif
	// TODO: Add ARMv8 Crypto Extensions check
	return false;
}


static status_t
DeriveKey(const BMessage& keyMessage, const uint8* salt, size_t saltSize,
	uint8* key, size_t keySize)
{
	BMallocIO buffer;
	status_t result = keyMessage.Flatten(&buffer);
	if (result != B_OK)
		return result;

	bool argon2Failed = true;

	// Attempt to use ARGON2ID if available
	EVP_KDF* kdf = EVP_KDF_fetch(NULL, "ARGON2ID", NULL);
	if (kdf != NULL) {
		EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
		EVP_KDF_free(kdf);

		if (kctx != NULL) {
			OSSL_PARAM params[6];
			const int threads = 1;
			const int lanes = 1;
			const int memory_cost = 65536; // 64 MiB
			const int iterations = 3;

			OSSL_PARAM* p = params;
			*p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_THREADS, (int*)&threads);
			*p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_LANES, (int*)&lanes);
			*p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_ITER, (int*)&iterations);
			*p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MEMCOST, (int*)&memory_cost);
			*p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
				(void*)salt, saltSize);
			*p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
				(void*)buffer.Buffer(), buffer.BufferLength());
			*p = OSSL_PARAM_construct_end();

			if (EVP_KDF_derive(kctx, key, keySize, params) > 0)
				argon2Failed = false;

			EVP_KDF_CTX_free(kctx);
		}
	}

	if (!argon2Failed)
		return B_OK;

	// Fallback to PBKDF2 with HMAC-SHA256
	// 600,000 iterations recommended for PBKDF2-HMAC-SHA256 (OWASP 2023)
	const int kIterations = 600000;
	if (PKCS5_PBKDF2_HMAC((const char*)buffer.Buffer(), buffer.BufferLength(),
			salt, saltSize, kIterations, EVP_sha256(), keySize, key) != 1) {
		return B_ERROR;
	}

	return B_OK;
}
#endif


status_t
Keyring::_EncryptToFlatBuffer()
{
	if (!fModified)
		return B_OK;

	if (!fUnlocked)
		return B_NOT_ALLOWED;

	BMessage container;
	status_t result = container.AddMessage("data", &fData);
	if (result != B_OK)
		return result;

	result = container.AddMessage("applications", &fApplications);
	if (result != B_OK)
		return result;

	BMallocIO plainBuffer;
	result = container.Flatten(&plainBuffer);
	if (result != B_OK)
		return result;

	fFlatBuffer.SetSize(0);
	fFlatBuffer.Seek(0, SEEK_SET);

#ifdef HAVE_OPENSSL
	if (fHasUnlockKey) {
		// Determine algorithm based on hardware support
		bool useAES = HasHardwareAES();
		const EVP_CIPHER* cipher = useAES ? EVP_aes_256_gcm()
			: EVP_chacha20_poly1305();

		const size_t kKeySize = 32;
		const size_t kSaltSize = 16;
		const size_t kIVSize = 12; // GCM and ChaCha20-Poly1305 standard IV
		const size_t kTagSize = 16;

		uint8 salt[kSaltSize];
		if (RAND_bytes(salt, sizeof(salt)) != 1)
			return B_ERROR;

		uint8 key[kKeySize];
		result = DeriveKey(fUnlockKey, salt, sizeof(salt), key, sizeof(key));
		if (result != B_OK)
			return result;

		uint8 iv[kIVSize];
		if (RAND_bytes(iv, sizeof(iv)) != 1)
			return B_ERROR;

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL)
			return B_NO_MEMORY;

		status_t status = B_OK;
		if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1
			|| EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
			status = B_ERROR;
		}

		uint8* ciphertext = NULL;
		int len = 0;
		int ciphertextLen = 0;

		if (status == B_OK) {
			// Write metadata: Algo ID + Salt + IV
			uint8 algoID = useAES ? 0 : 1;
			if (fFlatBuffer.Write(&algoID, sizeof(algoID)) != sizeof(algoID)
				|| fFlatBuffer.Write(salt, sizeof(salt)) != (ssize_t)sizeof(salt)
				|| fFlatBuffer.Write(iv, sizeof(iv)) != (ssize_t)sizeof(iv)) {
				status = B_ERROR;
			}
		}

		if (status == B_OK) {
			ciphertext = (uint8*)malloc(plainBuffer.BufferLength()
				+ EVP_CIPHER_block_size(cipher));
			if (ciphertext == NULL)
				status = B_NO_MEMORY;
		}

		if (status == B_OK) {
			if (EVP_EncryptUpdate(ctx, ciphertext, &len,
					(const uint8*)plainBuffer.Buffer(),
					plainBuffer.BufferLength()) != 1) {
				status = B_ERROR;
			}
			ciphertextLen = len;
		}

		if (status == B_OK) {
			if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1)
				status = B_ERROR;
			ciphertextLen += len;
		}

		if (status == B_OK) {
			// Get the tag (EVP_CTRL_GCM_GET_TAG works for ChaCha20-Poly1305 too)
			uint8 tag[kTagSize];
			if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, kTagSize, tag)
					!= 1) {
				status = B_ERROR;
			} else {
				// Write ciphertext + tag
				if (fFlatBuffer.Write(ciphertext, ciphertextLen)
						!= (ssize_t)ciphertextLen
					|| fFlatBuffer.Write(tag, sizeof(tag))
						!= (ssize_t)sizeof(tag)) {
					status = B_ERROR;
				}
			}
		}

		free(ciphertext);
		EVP_CIPHER_CTX_free(ctx);

		if (status != B_OK) {
			fFlatBuffer.SetSize(0);
			return status;
		}
	} else {
#endif
		// Just copy plaintext if no key or no OpenSSL
		if (fFlatBuffer.Write(plainBuffer.Buffer(), plainBuffer.BufferLength())
				!= (ssize_t)plainBuffer.BufferLength())
			return B_ERROR;
#ifdef HAVE_OPENSSL
	}
#endif

	fModified = false;
	return B_OK;
}


status_t
Keyring::_DecryptFromFlatBuffer()
{
	if (fFlatBuffer.BufferLength() == 0)
		return B_OK;

	BMallocIO* inputBuffer = &fFlatBuffer;
	BMallocIO decryptedBuffer;

#ifdef HAVE_OPENSSL
	if (fHasUnlockKey) {
		const size_t kKeySize = 32;
		const size_t kSaltSize = 16;
		const size_t kIVSize = 12;
		const size_t kTagSize = 16;
		const size_t kHeaderSize = sizeof(uint8) + kSaltSize + kIVSize;

		if (fFlatBuffer.BufferLength() < kHeaderSize + kTagSize)
			return B_ERROR;

		fFlatBuffer.Seek(0, SEEK_SET);

		uint8 algoID;
		if (fFlatBuffer.Read(&algoID, sizeof(algoID)) != sizeof(algoID))
			return B_ERROR;

		const EVP_CIPHER* cipher = NULL;
		if (algoID == 0)
			cipher = EVP_aes_256_gcm();
		else if (algoID == 1)
			cipher = EVP_chacha20_poly1305();
		else
			return B_BAD_VALUE; // Unknown algorithm

		uint8 salt[kSaltSize];
		if (fFlatBuffer.Read(salt, sizeof(salt)) != (ssize_t)sizeof(salt))
			return B_ERROR;

		uint8 iv[kIVSize];
		if (fFlatBuffer.Read(iv, sizeof(iv)) != (ssize_t)sizeof(iv))
			return B_ERROR;

		uint8 key[kKeySize];
		status_t result = DeriveKey(fUnlockKey, salt, sizeof(salt), key,
			sizeof(key));
		if (result != B_OK)
			return result;

		size_t dataSize = fFlatBuffer.BufferLength() - kHeaderSize - kTagSize;
		uint8* buffer = (uint8*)fFlatBuffer.Buffer() + kHeaderSize;
		uint8* tag = buffer + dataSize;

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL)
			return B_NO_MEMORY;

		status_t status = B_OK;
		if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1
			|| EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
			status = B_ERROR;
		}

		uint8* plaintext = NULL;
		int len = 0;
		int plaintextLen = 0;

		if (status == B_OK) {
			plaintext = (uint8*)malloc(dataSize
				+ EVP_CIPHER_block_size(cipher));
			if (plaintext == NULL)
				status = B_NO_MEMORY;
		}

		if (status == B_OK) {
			if (EVP_DecryptUpdate(ctx, plaintext, &len, buffer, dataSize)
					!= 1) {
				status = B_ERROR;
			}
			plaintextLen = len;
		}

		if (status == B_OK) {
			// Set expected tag
			if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, kTagSize, tag)
					!= 1) {
				status = B_ERROR;
			}
		}

		if (status == B_OK) {
			if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1)
				status = B_ERROR; // Tag verification failed
			plaintextLen += len;
		}

		if (status == B_OK) {
			decryptedBuffer.SetSize(plaintextLen);
			decryptedBuffer.WriteAt(0, plaintext, plaintextLen);
			inputBuffer = &decryptedBuffer;
		}

		free(plaintext);
		EVP_CIPHER_CTX_free(ctx);

		if (status != B_OK)
			return status;
	}
#endif

	BMessage container;
	inputBuffer->Seek(0, SEEK_SET);
	status_t result = container.Unflatten(inputBuffer);
	if (result != B_OK)
		return result;

	result = container.FindMessage("data", &fData);
	if (result != B_OK)
		return result;

	result = container.FindMessage("applications", &fApplications);
	if (result != B_OK) {
		fData.MakeEmpty();
		return result;
	}

	return B_OK;
}
