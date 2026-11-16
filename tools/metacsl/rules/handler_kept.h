/*
 * VERIFY-086: core/primitives/contract.h's rule, as a postcondition of
 * every function but contract_set_handler: the handler is the same when
 * the function returns. This form does not take the handler's address,
 * which no code of the library takes either, so that WP can keep the
 * handler out of memory; it runs apart from handler_written.h, which takes
 * it. Only where contract.h is included.
 */
#ifdef CANON_CORE_PRIMITIVES_CONTRACT_H
/*@ meta \prop,
      \name(handler_kept_by_all_but_its_setter),
      \targets(\diff(\ALL, {contract_set_handler})),
      \context(\postcond),
      canon_contract_handler == \old(canon_contract_handler);
*/
#endif
