/*
 *  Copyright (c) 2008 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef _NET80211_IEEE80211_CRYPTO_H_
#define _NET80211_IEEE80211_CRYPTO_H_

#include <wbuf.h>

#include "_ieee80211.h"
#include "ieee80211_rsn.h"
#include "ieee80211_wai.h"

struct ieee80211_cipher;

/*
 * Crypto key state.  There is sufficient room for all supported
 * ciphers (see below).  The underlying ciphers are handled
 * separately through loadable cipher modules that register with
 * the generic crypto support.  A key has a reference to an instance
 * of the cipher; any per-key state is hung off wk_private by the
 * cipher when it is attached.  Ciphers are automatically called
 * to detach and cleanup any such state when the key is deleted.
 *
 * The generic crypto support handles encap/decap of cipher-related
 * frame contents for both hardware- and software-based implementations.
 * A key requiring software crypto support is automatically flagged and
 * the cipher is expected to honor this and do the necessary work.
 * Ciphers such as TKIP may also support mixed hardware/software
 * encrypt/decrypt and MIC processing.
 */
/* XXX need key index typedef */
/* XXX pack better? */
/* XXX 48-bit rsc/tsc */
struct ieee80211_key {
    u_int8_t	wk_keylen;	/* key length in bytes */

    u_int8_t	wk_flags;
#define	IEEE80211_KEY_XMIT	0x01	/* key used for xmit */
#define	IEEE80211_KEY_RECV	0x02	/* key used for recv */
#define	IEEE80211_KEY_GROUP	0x04	/* key used for WPA group operation */
#define IEEE80211_KEY_MFP   0x08    /* key also used for management frames */
#define	IEEE80211_KEY_SWCRYPT	0x10	/* host-based encrypt/decrypt */
#define	IEEE80211_KEY_SWMIC	0x20	/* host-based enmic/demic */
#define IEEE80211_KEY_PERSISTENT 0x40   /* do not remove unless OS commands us to do so */
#define IEEE80211_KEY_PERSTA    0x80    /* per STA default key */

    int         wk_valid;
    u_int16_t	wk_keyix;	/* key index */
    u_int8_t	wk_key[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
#define	wk_txmic	wk_key+IEEE80211_KEYBUF_SIZE+0	/* XXX can't () right */
#define	wk_rxmic	wk_key+IEEE80211_KEYBUF_SIZE+8	/* XXX can't () right */
/*support for WAPI*/
#if ATH_SUPPORT_WAPI
    u_int8_t    wk_recviv[16];          /* key receive IV for WAPI*/
    u_int32_t   wk_txiv[4];          /* key transmit IV for WAPI*/
#endif    

    u_int64_t	wk_keyrsc[IEEE80211_TID_SIZE];	/* key receive sequence counter */
    u_int64_t   wk_keyrsc_suspect[IEEE80211_TID_SIZE]; /* Key receive sequence counter suspected */
    u_int64_t   wk_keyglobal; /* key receive global sequnce counter */
    u_int64_t	wk_keytsc;	/* key transmit sequence counter */
    const struct ieee80211_cipher *wk_cipher;
    void		*wk_private;	/* private cipher state */
    u_int16_t   wk_clearkeyix; /* index of clear key entry, needed for MFP */
};

/* common flags passed in by apps */           
#define	IEEE80211_KEY_COMMON 	(               \
        IEEE80211_KEY_XMIT |                    \
        IEEE80211_KEY_RECV |                    \
        IEEE80211_KEY_GROUP |                   \
        IEEE80211_KEY_PERSISTENT |              \
        IEEE80211_KEY_PERSTA |                  \
        IEEE80211_KEY_MFP |                     \
        0)

#define IEEE80211_IS_KEY_PERSISTENT(_key) \
    (((_key)->wk_flags & IEEE80211_KEY_PERSISTENT) == IEEE80211_KEY_PERSISTENT)

// This macro can only be used on keys after _newkey() has been called on them
#define IEEE80211_IS_KEY_PERSTA_SW(_key) \
    (((_key)->wk_flags & IEEE80211_KEY_PERSTA) && ((_key)->wk_keyix < IEEE80211_WEP_NKID))

#define IEEE80211_DEFAULT_KEYIX 0

struct ieee80211com;
struct ieee80211vap;
struct ieee80211_node;
struct ieee80211_rx_status;

void	ieee80211_crypto_attach(struct ieee80211com *);
void	ieee80211_crypto_detach(struct ieee80211com *);
void	ieee80211_crypto_vattach(struct ieee80211vap *);
void	ieee80211_crypto_vdetach(struct ieee80211vap *);
int	ieee80211_crypto_newkey(struct ieee80211vap *,
		int cipher, int flags, struct ieee80211_key *);
int	ieee80211_crypto_delkey(struct ieee80211vap *,
		struct ieee80211_key *,
		struct ieee80211_node *);
void ieee80211_crypto_freekey(struct ieee80211vap *, struct ieee80211_key *);
int	ieee80211_crypto_setkey(struct ieee80211vap *,
                                struct ieee80211_key *,
                                const u_int8_t bssid[IEEE80211_ADDR_LEN],
                                struct ieee80211_node *);

void	ieee80211_crypto_delglobalkeys(struct ieee80211vap *);

/*
 * Template for a supported cipher.  Ciphers register with the
 * crypto code and are typically loaded as separate modules
 * (the null cipher is always present).
 * XXX may need refcnts
 */
struct ieee80211_cipher {
	const char *ic_name;		/* printable name */
	u_int	ic_cipher;		/* IEEE80211_CIPHER_* */
	u_int	ic_header;		/* size of privacy header (bytes) */
	u_int	ic_trailer;		/* size of privacy trailer (bytes) */
	u_int	ic_miclen;		/* size of mic trailer (bytes) */
	void*	(*ic_attach)(struct ieee80211vap *, struct ieee80211_key *);
	void	(*ic_detach)(struct ieee80211_key *);
	int	(*ic_setkey)(struct ieee80211_key *);
	int	(*ic_encap)(struct ieee80211_key *, wbuf_t, u_int8_t keyid);
	int	(*ic_decap)(struct ieee80211_key *, wbuf_t, int, struct ieee80211_rx_status *);
	int	(*ic_enmic)(struct ieee80211_key *, wbuf_t, int);
	int	(*ic_demic)(struct ieee80211_key *, wbuf_t, int, int, struct ieee80211_rx_status *);
};
extern	const struct ieee80211_cipher ieee80211_cipher_none;

void	ieee80211_crypto_register(struct ieee80211com *ic, const struct ieee80211_cipher *);
void	ieee80211_crypto_unregister(struct ieee80211com *ic, const struct ieee80211_cipher *);
int	ieee80211_crypto_available(struct ieee80211com *ic, u_int cipher);

struct ieee80211_key *ieee80211_crypto_encap(struct ieee80211_node *, wbuf_t);
struct ieee80211_key *ieee80211_crypto_decap(struct ieee80211_node *, wbuf_t, int, struct ieee80211_rx_status *);

/*
 * Check and remove any MIC.
 */
static INLINE int
ieee80211_crypto_demic(struct ieee80211vap *vap, struct ieee80211_key *k,
                       wbuf_t wbuf, int hdrlen, int force, struct ieee80211_rx_status *rs)
{
	const struct ieee80211_cipher *cip = k->wk_cipher;
	return (cip->ic_miclen > 0 ? cip->ic_demic(k, wbuf, hdrlen, force, rs) : 1);
}

/*
 * Add any MIC.
 */
static INLINE int
ieee80211_crypto_enmic(struct ieee80211vap *vap,
                       struct ieee80211_key *k, wbuf_t wbuf, int force)
{
	const struct ieee80211_cipher *cip = k->wk_cipher;
	return (cip->ic_miclen > 0 ? cip->ic_enmic(k, wbuf, force) : 1);
}

/* 
 * Reset key state to an unused state.  The crypto
 * key allocation mechanism insures other state (e.g.
 * key data) is properly setup before a key is used.
 * KH:  But don't we really need to plumb a null key into the cache?
 */
static INLINE void
ieee80211_crypto_resetkey(struct ieee80211vap *vap, struct ieee80211_key *k, u_int16_t ix)
{
    k->wk_cipher = &ieee80211_cipher_none;
    k->wk_private = k->wk_cipher->ic_attach(vap, k);
    k->wk_keyix = ix;
    k->wk_valid = FALSE;
    k->wk_flags = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
}

/*
 * Crypto-related notification methods.
 */
void ieee80211_notify_replay_failure(struct ieee80211vap *,
                                     const struct ieee80211_frame *,
                                     const struct ieee80211_key *, u_int64_t rsc);
void ieee80211_notify_michael_failure(struct ieee80211vap *,
                                      const struct ieee80211_frame *, u_int keyix);

void ieee80211_crypto_register_none(struct ieee80211com *ic);
void ieee80211_crypto_unregister_none(struct ieee80211com *ic);

void ieee80211_crypto_register_wep(struct ieee80211com *ic);
void ieee80211_crypto_unregister_wep(struct ieee80211com *ic);

void ieee80211_crypto_register_tkip(struct ieee80211com *ic);
void ieee80211_crypto_unregister_tkip(struct ieee80211com *ic);

void ieee80211_crypto_register_ccmp(struct ieee80211com *ic);
void ieee80211_crypto_unregister_ccmp(struct ieee80211com *ic);

#if ATH_SUPPORT_WAPI
void ieee80211_crypto_register_sms4(struct ieee80211com *ic);
void ieee80211_crypto_unregister_sms4(struct ieee80211com *ic);
#endif

#if ATH_WOW
void ieee80211_find_iv_lengths(struct ieee80211vap *vap, 
                               u_int32_t *pBrKeyIVLength, u_int32_t *pUniKeyIVLength);
#endif

#if ATH_SUPPORT_IBSS_PRIVATE_SECURITY
#if ATH_SUPPORT_IBSS_WPA2
#define IEEE80211_CRYPTO_KEY(_vap, _ni, _id) &_ni->ni_persta.nips_swkey[_id >> 6]
#else
#define IEEE80211_CRYPTO_KEY(_vap, _ni, _id) &vap->iv_nw_keys[_id >> 6]
#endif
#else
#define IEEE80211_CRYPTO_KEY(_vap, _ni, _id) &_ni->ni_persta.nips_swkey[_id >> 6]
#endif

#endif /* _NET80211_IEEE80211_CRYPTO_H_ */