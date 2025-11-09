# What is this?
This repository contains the source code for the Trajan kernel, part of the Trajan operating system.
All code in this repository is licenced under the terms of the MIT Licence, contained in ``LICENCE``.
### Contributing
Contributions are welcome, however, they are subject to the following conditions:
- GPL Licenced code is BANNED. No exceptions.
- All code MUST follow the 1TBS coding style. Further information can be found in ``STYLE.md``
- If you so happen to find code that does not follow the coding style, then please reformat it and submit a pull request.
- Keep comments to a minimum.
- GNU code is BANNED. An exception are the standard headers (``stdint.h``, ``stddef.h``) and friends, because they are compiler-provided.
- Proprietary code must follow the following form:
    - Open-source "glue" code goes under ``src/``
    - Proprietary code and blobs go under ``nosup/``
- Using the GNU Coding style will result in you getting banned from future contributions.
- the ONLY languages allowed under ``src/`` are C, C++, and nasm-style x86_64 assembly. Rust, and anything else are banned. 
- drivers in languages that are banned should go under ``nosup/``
- dynamically loaded drivers and modules should go under ``mods/``
We do not have a set Code of Conduct. pull requests and rollbacks are at the sole discretion of the Trajan Maintainers.
We do not accept patches that are not in the git-patch format. We heavily discourage using patches, instead, please use pull requests with gitea.
Commits to ``master`` are to be reviewed by at least two maintainers, and if not available, me (aurelian).
Have fun!
