# Security policy

Komodo is a compiler and runtime; the most relevant issues are memory-safety
bugs in generated code or in `stdlib/*.c`, and unsafe handling of untrusted
`.km` source or C headers (`tools/km-bind`).

**Please report vulnerabilities privately** by email to lukasperks@gmail.com
(or via GitHub's "Report a vulnerability" under the Security tab), with a
minimal program that reproduces the problem. Expect an acknowledgement within
a week. Please don't open a public issue until a fix is available.

Supported versions: the latest release and `main`.
