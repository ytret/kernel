{
  description = "GCC cross-compiler toolchain for i686-elf";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";

  outputs =
    { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems =
        f: nixpkgs.lib.genAttrs supportedSystems (system: f system (import nixpkgs { inherit system; }));

      target = "i686-elf";
      binutilsVersion = "2.45";
      gccVersion = "15.2.0";
    in
    {
      packages = forAllSystems (
        system: pkgs:
        let

          i686-elf-binutils = pkgs.stdenv.mkDerivation {
            pname = "i686-elf-binutils";
            version = binutilsVersion;

            src = pkgs.fetchurl {
              url = "https://sourceware.org/pub/binutils/releases/binutils-${binutilsVersion}.tar.xz";
              sha256 = "sha256-xQwOf5yxiJgOLMl+RTdiaxZyRBgVWH8eq2nSob++9dI=";
            };

            nativeBuildInputs = with pkgs; [ texinfo ];

            separateDebugInfo = false;
            separateBuildDir = true;

            configureFlags = [
              "--target=${target}"
              "--prefix=${placeholder "out"}"
              "--with-sysroot"
              "--disable-nls"
              "--disable-werror"
            ];

            buildPhase = "make -j$NIX_BUILD_CORES";
            installPhase = "make install";

            meta = {
              description = "GNU Binutils ${binutilsVersion} cross-assembled for ${target}";
              homepage = "https://www.gnu.org/software/binutils/";
              license = pkgs.lib.licenses.gpl3Plus;
              platforms = pkgs.lib.platforms.unix;
            };
          };

          i686-elf-gcc = pkgs.stdenv.mkDerivation {
            pname = "i686-elf-gcc";
            version = gccVersion;

            hardeningDisable = [
              "format"
            ];

            src = pkgs.fetchurl {
              url = "https://ftp.gnu.org/gnu/gcc/gcc-${gccVersion}/gcc-${gccVersion}.tar.xz";
              sha256 = "sha256-Q4/ZloJrDIJIWinaA6ctcdbjVBqD7HAt9Ccfb+Al0k4=";
            };

            nativeBuildInputs = with pkgs; [
              i686-elf-binutils
              texinfo
              gmp
              libmpc
              mpfr
              isl
              flex
              bison
              file
            ];

            buildInputs = with pkgs; [
              i686-elf-binutils
              gmp
              libmpc
              mpfr
              isl
            ];

            # See https://gcc.gnu.org/install/configure.html
            configurePhase = ''
              mkdir ../build
              cd ../build
              ../gcc-15.2.0/configure \
                  --target=${target} \
                  --prefix="${placeholder "out"}" \
                  --with-local-prefix="${placeholder "out"}" \
                  --disable-nls \
                  --enable-languages=c \
                  --without-headers \
                  --with-as="${i686-elf-binutils}/bin/${target}-as" \
                  --with-ld="${i686-elf-binutils}/bin/${target}-ld"
            '';

            buildPhase = ''
              make -j$NIX_BUILD_CORES all-gcc
              make -j$NIX_BUILD_CORES all-target-libgcc
            '';

            installPhase = ''
              make install-gcc
              make install-target-libgcc
            '';

            meta = {
              description = "GCC ${gccVersion} C/C++ cross-compiler targeting ${target}";
              homepage = "https://gcc.gnu.org/";
              license = pkgs.lib.licenses.gpl3Plus;
              platforms = pkgs.lib.platforms.unix;
            };
          };

        in
        {
          inherit i686-elf-binutils i686-elf-gcc;
        }
      );

      devShells = forAllSystems (
        system: pkgs:
        let
          minimalPackages = [
            self.packages.${system}.i686-elf-binutils
            self.packages.${system}.i686-elf-gcc
          ];
          buildingPackages = [
            pkgs.cmake
            pkgs.ninja
          ];
          bootableIsoPackages = [
            pkgs.grub2
            pkgs.libisoburn
            pkgs.mtools
          ];
          virtualDiskPackages = [
            pkgs.qemu-utils
            pkgs.e2fsprogs
          ];
          emulatorPackages = [
            pkgs.qemu_full
          ];
          fullPackages =
            minimalPackages
            ++ buildingPackages
            ++ bootableIsoPackages
            ++ virtualDiskPackages
            ++ emulatorPackages;
          pkgName = p: p.pname or p.name or "unknown";
        in
        rec {
          minimal = pkgs.mkShell rec {
            packages = minimalPackages;
            shellHook = ''
              echo "Packages in this shell:"
              echo "${builtins.concatStringsSep ", " (map pkgName packages)}"
            '';
          };

          full = pkgs.mkShell rec {
            packages = fullPackages;
            shellHook = ''
              echo "Packages in this shell:"
              echo "${builtins.concatStringsSep ", " (map pkgName packages)}"
            '';
          };

          default = full;
        }
      );
    };
}
