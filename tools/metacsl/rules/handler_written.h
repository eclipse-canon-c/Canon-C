/*
 * VERIFY-086: core/primitives/contract.h's rule at every write.
 * canon_contract_handler is "the library's ONLY global mutable object", to
 * be written "through contract_set_handler() only". MetAcsl puts a check
 * before each write of every function but contract_set_handler, and at
 * each call of a function without a body, for each location its assigns
 * clause names: the location written is not the handler. Only where
 * contract.h is included; elsewhere there is no handler to write.
 */
#ifdef CANON_CORE_PRIMITIVES_CONTRACT_H
/*@ meta \prop,
      \name(handler_written_only_by_its_setter),
      \targets(\diff(\ALL, {contract_set_handler})),
      \context(\writing),
      \separated(\written, &canon_contract_handler);
*/
#endif
