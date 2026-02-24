{lib, stdenv, fetchFromGitHub}:

stdenv.mkDerivation rec {
  pname = "boost-ut";
  version = "2.3.1";

  src = fetchFromGitHub {
    owner = "boost-ext";
    repo = "ut";
    rev = "v${version}";
    hash = "sha256-VCTrs0CMr4pUrJ2zEsO8s7j16zOkyDNhBc5zw0rAAZI=";
  };

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/include $out/lib/pkgconfig

    cp -r include/boost $out/include/

    cat > $out/lib/pkgconfig/boost.ut.pc << EOF
    Name: boost.ut
    Description: C++20 unit testing framework
    Version: ${version}
    Cflags: -I$out/include
    EOF

    runHook postInstall
  '';

  meta = {
    description = "C++20 unit testing framework (Boost.UT)";
    homepage = "https://github.com/boost-ext/ut";
    license = lib.licenses.boost;
    platforms = lib.platforms.all;
  };
}
