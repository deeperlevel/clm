pub mod clm;
#[cfg(not(test))]
#[path = "tests/test.rs"]
mod test;

#[cfg(not(test))]
fn main() {
    if !test::run_all_tests() {
        std::process::exit(1);
    }
}
