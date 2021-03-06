/*
 * Generated by asn1c-0.9.22.1409 (http://lionet.info/asn1c)
 * From ASN.1 module "SPNEGO"
 * 	found in "spnego.asn1"
 * 	`asn1c -fnative-types -fskeletons-copy`
 */

#ifndef	_MechType_H_
#define	_MechType_H_


#include <asn_application.h>

/* Including external dependencies */
#include <OBJECT_IDENTIFIER.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MechType */
typedef OBJECT_IDENTIFIER_t	 MechType_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_MechType;
asn_struct_free_f MechType_free;
asn_struct_print_f MechType_print;
asn_constr_check_f MechType_constraint;
ber_type_decoder_f MechType_decode_ber;
der_type_encoder_f MechType_encode_der;
xer_type_decoder_f MechType_decode_xer;
xer_type_encoder_f MechType_encode_xer;
per_type_decoder_f MechType_decode_uper;
per_type_encoder_f MechType_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _MechType_H_ */
