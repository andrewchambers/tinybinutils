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
      ./tinyas-x86_64 --version

      cat > plt-suffix.s <<'EOF'
      .globl _start
      _start:
        call puts@PLT
      EOF

      cat > unsupported-suffix.s <<'EOF'
      .globl _start
      _start:
        mov puts@GOTPCREL(%rip), %rax
      EOF

      ./tinyas-x86_64 -o plt-suffix.o plt-suffix.s
      if ./tinyas-x86_64 -o unsupported-suffix.o unsupported-suffix.s 2>unsupported-suffix.err; then
        echo "expected unsupported suffix failure"
        exit 1
      fi
      grep "unsupported symbol suffix '@GOTPCREL'" unsupported-suffix.err

      cat > add.c <<'EOF'
      int add(int a, int b) { return a + b; }
      EOF

      cat > main.c <<'EOF'
      extern int add(int, int);
      int main(void) { return add(20, 22) == 42 ? 0 : 1; }
      EOF

      ${cprocHost}/bin/cproc -S -o add.s add.c
      ${cprocHost}/bin/cproc -S -o main.s main.c

      ./tinyas-x86_64 -o add.o add.s
      ./tinyas-x86_64 -o main.o main.s

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
      cp tinyld tinyld-x86_64 tinyld-aarch64 tinyld-riscv64 \
        tinyas tinyas-x86_64 tinyas-aarch64 tinyas-riscv64 tinyar \
        $out/bin/
      runHook postInstall
    '';

    passthru = {
      inherit musl cprocHost;
    };
  };

in
tinybinutilsSelf
