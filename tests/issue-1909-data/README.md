Fixtures for `tests/issue-1909.ts`.

From the test resources of [ofdrw](https://github.com/ofdrw/ofdrw) (MIT), unchanged: `SESV1SignDoc.ofd` and
`SESV4SignDoc.ofd` (SES seals), `GB35275PKCS9DigitalSign.ofd` (GB/T 35275 signature), `DigitalSign.ofd` (a bare
signature without a certificate) and `hello-integrity.ofd` (GM/T 0099 integrity protection).

Generated (need OpenSSL 3):

- `encrypted.ofd`: password `sumatra`, by `bun tests/issue-1909-data/make-encrypted.ts`
- `chain-signed.ofd` and `chain-root.pem`: an SES seal whose signer is issued by that test root, by
  `bun tests/issue-1909-data/make-signed.ts`
