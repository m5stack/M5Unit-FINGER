# M5Unit - FINGER

## Overview

Library for Finger units using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU:U008
Unit Finger is a fingerprint recognition sensor. It integrates the FPC1020A capacitive fingerprint recognition module, featuring multi-fingerprint entry, image processing, feature extraction, fingerprint comparison, and search functions.

### SKU:U074

Hat Finger is a fingerprint recognition sensor. It integrates the FPC1020SC capacitive fingerprint recognition module, featuring multi-fingerprint entry, image processing, feature extraction, fingerprint matching, and search functions. 


### SKU:U203
The Unit Fingerprint2 is a high-performance fingerprint recognition sensor unit, internally integrating an STM32 core controller and an A-K323CP all-in-one fingerprint recognition module. It uses a semiconductor capacitive sensor with functions such as fingerprint acquisition, feature extraction, registration, comparison, storage, and retrieval. 

## Related Link
See also examples using conventional methods here.

- [Unit Finger Document & Datasheet](https://docs.m5stack.com/en/unit/finger)
- [Hat Finger Document & Datasheet](https://docs.m5stack.com/en/hat/hat-finger)
- [Unit Fingerprint2 Document & Datasheet](https://docs.m5stack.com/en/unit/Unit_Fingerprint2)


## Required Libraries:

- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)


## License

- [M5Unit-FINGER - MIT](LICENSE)

## Examples
See also [examples/UnitUnified](examples/UnitUnified)

## Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-FINGER/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

### Required
- [Doxyegn](https://www.doxygen.nl/)
- [pcregrep](https://formulae.brew.sh/formula/pcre2)
- [Git](https://git-scm.com/) (Output commit hash to html)


