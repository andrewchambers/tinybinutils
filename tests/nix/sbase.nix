{ pkgs ? import ./nixpkgs.nix
, testenv ? import ./testenv.nix { inherit pkgs; }
}:

let
  inherit (pkgs) stdenv fetchgit;
  inherit (testenv) commonNativeBuildInputs cprocHost tinybinutilsHost;
in
stdenv.mkDerivation {
  pname = "tinybinutils-test-sbase";
  version = "git-c134158";

  src = fetchgit {
    url = "https://git.suckless.org/sbase";
    rev = "c1341583c96307cb0e6152c963ed23c4d56a4278";
    sha256 = "sha256-v6VAi8Xv5cygN3z+4OqLTRlOH98sskt+v4i1x0xI65Q=";
  };

  nativeBuildInputs = commonNativeBuildInputs ++ [
    pkgs.byacc
  ];

  postPatch = ''
    patchShebangs scripts
    substituteInPlace make/main.c \
      --replace-fail "volatile sig_atomic_t  stop;" "sig_atomic_t stop;"
    substituteInPlace make/make.h \
      --replace-fail "extern volatile sig_atomic_t stop;" "extern sig_atomic_t stop;"
    substituteInPlace make/posix.c \
      --replace-fail "static volatile pid_t pid;" "static pid_t pid;"
  '';

  dontPatchELF = true;

  buildPhase = ''
    runHook preBuild
    make \
      CC="${cprocHost}/bin/cproc" \
      AR="${tinybinutilsHost}/bin/tinyar" \
      ARFLAGS=crs \
      RANLIB=true
    runHook postBuild
  '';

  doCheck = true;
  checkPhase = ''
    runHook preCheck

    ./echo sbase-smoke | ./grep sbase-smoke

    ./printf 'beta\nalpha\n' > unsorted
    ./printf 'alpha\nbeta\n' > expected
    ./sort unsorted > sorted
    cmp expected sorted

    ./cat sorted | ./wc -l | ./grep '2'
    ./basename /tmp/sbase-smoke | ./grep '^sbase-smoke$'
    ./dirname /tmp/sbase-smoke | ./grep '^/tmp$'
    ./true
    ! ./false

    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    make \
      CC="${cprocHost}/bin/cproc" \
      AR="${tinybinutilsHost}/bin/tinyar" \
      ARFLAGS=crs \
      RANLIB=true \
      PREFIX=$out \
      MANPREFIX=$out/share/man \
      install
    runHook postInstall
  '';

  passthru = {
    inherit cprocHost tinybinutilsHost;
  };
}
