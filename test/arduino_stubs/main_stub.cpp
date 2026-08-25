// Link-only entry point for the example-sketch build check (see
// check_examples_compile.sh). Deliberately does NOT call the sketch's
// setup()/loop() -- this check exists to catch a missing declaration
// (compile error) or missing definition (undefined reference at link
// time) in the library API the examples call, exactly like an Arduino
// IDE "Verify" does. It is not a device simulator, so nothing here
// exercises WiFi/HTTP/deep-sleep at runtime.
int main()
{
    return 0;
}
