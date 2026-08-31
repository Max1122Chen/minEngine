use rand::Rng;

use crate::types::Guid;

pub fn generate_guid() -> Guid {
    let mut rng = rand::thread_rng();
    let mut high = rng.gen::<u64>();
    let low = rng.gen::<u64>();

    if high == 0 && low == 0 {
        high = 1;
    }

    Guid { high, low }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generates_non_zero_guid() {
        let guid = generate_guid();
        assert!(!guid.is_zero());
    }
}
