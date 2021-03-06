/*
 * Generated by asn1c-0.9.22.1409 (http://lionet.info/asn1c)
 * From ASN.1 module "SPNEGO"
 * 	found in "spnego.asn1"
 * 	`asn1c -fnative-types -fskeletons-copy`
 */

#ifndef	_ContextFlags_H_
#define	_ContextFlags_H_


#include <asn_application.h>

/* Including external dependencies */
#include <BIT_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum ContextFlags {
	ContextFlags_delegFlag	= 0,
	ContextFlags_mutualFlag	= 1,
	ContextFlags_replayFlag	= 2,
	ContextFlags_sequenceFlag	= 3,
	ContextFlags_anonFlag	= 4,
	ContextFlags_confFlag	= 5,
	ContextFlags_integFlag	= 6
} e_ContextFlags;

/* ContextFlags */
typedef BIT_STRING_t	 ContextFlags_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_ContextFlags;
asn_struct_free_f ContextFlags_free;
asn_struct_print_f ContextFlags_print;
asn_constr_check_f ContextFlags_constraint;
ber_type_decoder_f ContextFlags_decode_ber;
der_type_encoder_f ContextFlags_encode_der;
xer_type_decoder_f ContextFlags_decode_xer;
xer_type_encoder_f ContextFlags_encode_xer;
per_type_decoder_f ContextFlags_decode_uper;
per_type_encoder_f ContextFlags_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _ContextFlags_H_ */
