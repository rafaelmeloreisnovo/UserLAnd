# Standards Alignment (ISO/IEC, RFC, IEEE, NIST)

> **Nota/Note:** Este documento é um mapeamento de referência para orientar refatorações,
> interoperabilidade e confiabilidade. Ele **não** implica certificação formal.

## Objetivo / Goal

Consolidar normas e recomendações técnicas que guiam interoperabilidade, segurança,
confiabilidade e manutenção do projeto, com foco em Android 15 e estabilidade low-level.

---

## ISO/IEC Reference Set (30+)

1. **ISO 8000-1** — Data quality: Overview
2. **ISO 8000-8** — Data quality: Information and data quality
3. **ISO 8000-61** — Data quality: Data quality management
4. **ISO 9001** — Quality management systems: Requirements
5. **ISO 9004** — Quality management: Guidance for sustained success
6. **ISO/IEC 25010** — Systems/software quality models
7. **ISO/IEC 25012** — Data quality model
8. **ISO/IEC 25023** — System/software product quality measurement
9. **ISO/IEC 25040** — Software quality evaluation process
10. **ISO/IEC 12207** — Software life cycle processes
11. **ISO/IEC 15288** — System life cycle processes
12. **ISO/IEC 27001** — Information security management systems
13. **ISO/IEC 27002** — Information security controls
14. **ISO/IEC 27005** — Information security risk management
15. **ISO/IEC 27034** — Application security
16. **ISO/IEC 29100** — Privacy framework
17. **ISO/IEC 29151** — PII protection
18. **ISO/IEC 21827** — Systems Security Engineering (SSE-CMM)
19. **ISO/IEC 15408** — Common Criteria
20. **ISO/IEC 14764** — Software maintenance
21. **ISO/IEC 24765** — Systems and software engineering vocabulary
22. **ISO/IEC 15939** — Measurement process
23. **ISO/IEC 33001** — Process assessment concepts
24. **ISO/IEC 33020** — Process measurement framework
25. **ISO/IEC 20000-1** — IT service management
26. **ISO/IEC 38500** — IT governance
27. **ISO/IEC 2382** — IT terminology
28. **ISO/IEC 27017** — Cloud security controls
29. **ISO/IEC 27018** — Cloud privacy
30. **ISO 31000** — Risk management guidelines
31. **ISO 22301** — Business continuity management
32. **ISO 22313** — Business continuity guidance
33. **ISO/IEC 26514** — Documentation for software and systems

---

## Normative References (20+)

1. **RFC 2119** — Key words for use in RFCs (MUST/SHOULD)
2. **RFC 8174** — Ambiguity of upper-case requirements
3. **RFC 8259** — JSON
4. **RFC 5234** — ABNF
5. **RFC 7230** — HTTP/1.1 Message Syntax and Routing
6. **RFC 9110** — HTTP Semantics
7. **RFC 9112** — HTTP/1.1
8. **RFC 8446** — TLS 1.3
9. **RFC 1035** — DNS
10. **RFC 5321** — SMTP
11. **IEEE 754** — Floating-point arithmetic
12. **IEEE 1003.1** — POSIX Base Specifications
13. **IEEE 1012** — System/Software Verification and Validation
14. **IEEE 1016** — Software Design Description
15. **IEEE 730** — Software Quality Assurance
16. **IEEE 829** — Test Documentation
17. **IEEE 830** — Software Requirements Specifications
18. **IEEE 1471 / ISO/IEC/IEEE 42010** — Architecture description
19. **NIST SP 800-53** — Security and privacy controls
20. **NIST SP 800-63** — Digital identity guidelines
21. **NIST SP 800-171** — Protecting CUI
22. **NIST SP 800-160** — Systems security engineering
23. **NIST SP 800-190** — Container security

---

## Aplicação / Application to the Codebase

- **Interoperabilidade:** uso de normas de dados e qualidade (ISO 8000, ISO/IEC 250xx).
- **Confiabilidade:** processos de manutenção e avaliação (ISO/IEC 14764, 25040).
- **Segurança:** controles e engenharia (ISO/IEC 27001/27002, NIST 800-53/160).
- **Compatibilidade:** aderência a RFCs/IEEE/Posix para parsers, rede e matemática.

---

## Próximos Passos / Next Steps

1. Expandir mapeamento com evidências por módulo.
2. Associar cada área do app a requisitos ISO/IEC e NIST aplicáveis.
3. Manter o documento sincronizado com mudanças de dependências.
