use std::sync::atomic::{AtomicUsize, Ordering};

static MAX_MODULE_WIDTH: AtomicUsize = AtomicUsize::new(15);

pub fn max_target_width(target: &str) -> usize {
    let max_width = MAX_MODULE_WIDTH.load(Ordering::Relaxed);
    if max_width < target.len() {
        MAX_MODULE_WIDTH.store(target.len(), Ordering::Relaxed);
        target.len()
    } else {
        max_width
    }
}
