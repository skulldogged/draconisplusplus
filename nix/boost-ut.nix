{lib, stdenv, fetchFromGitHub}:

stdenv.mkDerivation rec {
  pname = "boost-ut";
  version = "2.3.1";

  src = fetchFromGitHub {
    owner = "boost-ext";
    repo = "ut";
    rev = "v${version}";
    # Replace with the real hash when network access is available.
    hash = lib.fakeSha256;
  };

  sourceRoot = "ut-${version}";

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/include
    cp -r include/boost $out/include/

    runHook postInstall
  '';

  meta = {
    description = "C++20 unit testing framework (Boost.UT)";
    homepage = "https://github.com/boost-ext/ut";
    license = lib.licenses.boost;
    platforms = lib.platforms.all;
  };
}
