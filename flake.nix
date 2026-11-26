{
  outputs = { flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        inherit (pkgs.lib.fileset) toSource unions;

        mkTest = stdenv: stdenv.mkDerivation {
          pname = "ef-test-${stdenv.cc.cc.pname}";
          version = "0.0.1";

          src = toSource {
            root = ./.;
            fileset = unions [
              ./ef.c
              ./main.c
              ./meson.build
            ];
          };

          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
          ];

          buildInputs = with pkgs; [
            libblocksruntime
          ];

          preConfigure = ''
            meson rewrite kwargs set project / version "$version"
          '';

          mesonBuildType = "release";
          mesonFlags = [ "--werror" ];
        };
      in
      rec {
        packages = rec {
          clang = mkTest pkgs.clangStdenv;
          gcc = mkTest pkgs.gccStdenv;

          default = pkgs.linkFarmFromDrvs "ef-test" [ clang gcc ];
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = builtins.attrValues packages;

          packages = with pkgs; [
            (pkgs.writeShellScriptBin "clang" ''
              exec "${lib.getExe clang}" "$@"
            '')

            clang-tools
            gdb
            hyperfine
            just
            valgrind
          ];

          shellHook = ''
            if [ ! -f t8.shakespeare.txt ]; then
              curl -LO https://ocw.mit.edu/ans7870/6/6.006/s08/lecturenotes/files/t8.shakespeare.txt
            fi
          '';
        };
      });
}
