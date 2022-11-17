(use-modules (gnu) (guix))

(use-package-modules base
                     commencement
                     code
                     nvi
                     version-control
                     pkg-config
                     linux)

(define-public %fbusb-fuse-dev-packages
  (list gnu-make
        gcc-toolchain-7
        cscope
        nvi
        pkg-config
        git
        linux-libre-headers
        fuse-3))

(packages->manifest %fbusb-fuse-dev-packages)
