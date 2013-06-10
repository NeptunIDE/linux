#!/bin/bash
# apt-get install kernel-package
export CONCURRENCY_LEVEL="8"
make-kpkg --initrd kernel_image kernel_headers modules modules_image
