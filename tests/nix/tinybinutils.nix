{ pkgs ? import ./nixpkgs.nix
, testenv ? import ./testenv.nix { inherit pkgs; }
}:

let
  inherit (pkgs) stdenv;
  inherit (testenv) commonNativeBuildInputs tinybinutilsSrc musl cprocHost;

  tinybinutilsSelf = stdenv.mkDerivation {
    pname = "tinybinutils-self";
    version = "test";
    dontPatchELF = true;

    src = tinybinutilsSrc;
    nativeBuildInputs = commonNativeBuildInputs;

    buildPhase = ''
      runHook preBuild
      make CC="${cprocHost}/bin/cproc"
      runHook postBuild
    '';

    doCheck = true;
    checkPhase = ''
      runHook preCheck

      ./tinyld-x86_64 --version

      cat > add.c <<'EOF'
      int add(int a, int b) { return a + b; }
      EOF

      cat > main.c <<'EOF'
      extern int add(int, int);
      int main(void) { return add(20, 22) == 42 ? 0 : 1; }
      EOF

      ${cprocHost}/bin/cproc -c -o add.o add.c
      ${cprocHost}/bin/cproc -c -o main.o main.c

      ./tinyar crs libadd.a add.o
      ./tinyar t libadd.a | grep '^add.o$'

      ./tinyld-x86_64 \
        -static \
        -L . \
        -L ${musl}/lib \
        -o smoke \
        ${musl}/lib/crt1.o \
        ${musl}/lib/crti.o \
        main.o \
        -l:libadd.a \
        -lc \
        ${musl}/lib/crtn.o

      ./smoke

      runHook postCheck
    '';

    installPhase = ''
      runHook preInstall
      mkdir -p $out/bin
      cp tinyld tinyld-x86_64 tinyld-aarch64 tinyld-riscv64 tinyar $out/bin/
      runHook postInstall
    '';

    passthru = {
      inherit musl cprocHost;
    };
  };

in
tinybinutilsSelf
