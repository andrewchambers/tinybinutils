{ pkgs ? import ./nixpkgs.nix }:

let
  inherit (pkgs) lib stdenv fetchurl;

  target =
    if stdenv.hostPlatform.system == "x86_64-linux" then
      "x86_64-linux-musl"
    else
      throw "tinybinutils end-to-end test currently supports x86_64-linux only";

  qbeVersion = "1.2";
  cprocRev = "ee952381bac577239cc83f88d75d7688968cb91e";

  commonNativeBuildInputs = with pkgs; [
    coreutils
    diffutils
    gawk
    gnugrep
    gnumake
    gnused
  ];

  tinybinutilsSrc = lib.cleanSource ../..;

  musl = stdenv.mkDerivation {
    pname = "tinybinutils-test-musl";
    version = "1.2.5";
    dontPatchELF = true;

    src = fetchurl {
      url = "https://musl.libc.org/releases/musl-1.2.5.tar.gz";
      sha256 = "1r3mgky9d19b2285s274qxzlgs7sncx8plm01vd691sdx2xii8d9";
    };

    nativeBuildInputs = commonNativeBuildInputs;

    configurePhase = ''
      runHook preConfigure
      patchShebangs configure
      ./configure \
        --prefix=$out \
        --disable-shared \
        CC="${stdenv.cc}/bin/cc"
      runHook postConfigure
    '';

    buildPhase = ''
      runHook preBuild
      make
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      make install
      runHook postInstall
    '';
  };

  qbeHost = stdenv.mkDerivation {
    pname = "tinybinutils-test-qbe-host";
    version = qbeVersion;

    src = fetchurl {
      url = "https://c9x.me/compile/release/qbe-${qbeVersion}.tar.xz";
      sha256 = "0p92d19xzwcs5dgrhbcmmj1a7dvk3y3538bbyx5j6njjaawhxmd6";
    };

    nativeBuildInputs = commonNativeBuildInputs;

    postPatch = ''
      patchShebangs tools
      substituteInPlace tools/test.sh \
        --replace-fail 'tmp=/tmp/qbe.zzzz' 'tmp=''${TMPDIR:-/tmp}/qbe.zzzz'
    '';

    buildPhase = ''
      runHook preBuild
      make CC="${stdenv.cc}/bin/cc"
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      make install PREFIX=$out
      runHook postInstall
    '';
  };

  tinybinutilsHost = stdenv.mkDerivation {
    pname = "tinybinutils-host";
    version = "test";

    src = tinybinutilsSrc;
    nativeBuildInputs = commonNativeBuildInputs;

    buildPhase = ''
      runHook preBuild
      make CC="${stdenv.cc}/bin/cc"
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      mkdir -p $out/bin
      cp tinyld tinyld-x86_64 tinyld-aarch64 tinyld-riscv64 \
        tinyas tinyas-x86_64 tinyas-aarch64 tinyas-riscv64 tinyar \
        $out/bin/
      runHook postInstall
    '';
  };

  mkCproc = { pname, cc, qbe, tinybinutils, doBootstrap ? false }:
    stdenv.mkDerivation {
      inherit pname;
      version = "git-${builtins.substring 0 12 cprocRev}";

      src = fetchurl {
        url = "https://git.sr.ht/~mcf/cproc/archive/${cprocRev}.tar.gz";
        sha256 = "04bsy6az6givvjv8dplgxv0qvpyasvqjr00rhl03fbiznv6mc2pd";
      };

      nativeBuildInputs = commonNativeBuildInputs;

      postPatch = ''
        patchShebangs configure runtests
      '';

      configurePhase = ''
        runHook preConfigure
        ./configure \
          --prefix=$out \
          --target=${target} \
          --with-cpp="${stdenv.cc}/bin/cc -E -nostdinc -isystem ${musl}/include" \
          --with-qbe="${qbe}/bin/qbe" \
          --with-as="${tinybinutils}/bin/tinyas-x86_64" \
          --with-ld="${tinybinutils}/bin/tinyld-x86_64 -static -L ${musl}/lib" \
          --with-ldso= \
          CC="${cc}"
        runHook postConfigure
      '';

      buildPhase = ''
        runHook preBuild
        make
        ${lib.optionalString doBootstrap "make bootstrap"}
        runHook postBuild
      '';

      doCheck = true;
      checkPhase = ''
        runHook preCheck
        make check
        ${lib.optionalString doBootstrap "make check-stage2"}
        runHook postCheck
      '';

      installPhase = ''
        runHook preInstall
        mkdir -p $out/bin $out/share/man/man1
        ${if doBootstrap then
          "cp stage3/cproc stage3/cproc-qbe $out/bin/"
        else
          "cp cproc cproc-qbe $out/bin/"}
        cp cproc.1 $out/share/man/man1/
        runHook postInstall
      '';
    };

  cprocHost = mkCproc {
    pname = "tinybinutils-test-cproc-host";
    cc = "${stdenv.cc}/bin/cc";
    qbe = qbeHost;
    tinybinutils = tinybinutilsHost;
  };

  qbeSelf = stdenv.mkDerivation {
    pname = "tinybinutils-test-qbe-self";
    version = qbeVersion;

    src = fetchurl {
      url = "https://c9x.me/compile/release/qbe-${qbeVersion}.tar.xz";
      sha256 = "0p92d19xzwcs5dgrhbcmmj1a7dvk3y3538bbyx5j6njjaawhxmd6";
    };

    nativeBuildInputs = commonNativeBuildInputs;

    postPatch = ''
      patchShebangs tools
      substituteInPlace tools/test.sh \
        --replace-fail 'tmp=/tmp/qbe.zzzz' 'tmp=''${TMPDIR:-/tmp}/qbe.zzzz'
    '';

    buildPhase = ''
      runHook preBuild
      make CC="${cprocHost}/bin/cproc"
      runHook postBuild
    '';

    doCheck = true;
    checkPhase = ''
      runHook preCheck
      make check CC="${cprocHost}/bin/cproc"
      runHook postCheck
    '';

    installPhase = ''
      runHook preInstall
      make install PREFIX=$out
      runHook postInstall
    '';
  };

in
{
  inherit target qbeVersion cprocRev commonNativeBuildInputs tinybinutilsSrc
    musl qbeHost qbeSelf tinybinutilsHost mkCproc cprocHost;
}
