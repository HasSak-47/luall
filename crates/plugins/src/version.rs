use anyhow::*;
use serde::de::Error as DeError;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Version {
    major: usize,
    minor: usize,
    patch: usize,
    pre: Option<String>,
    build: Option<String>,
}

impl Default for Version {
    fn default() -> Self {
        return Version {
            major: 0,
            minor: 1,
            patch: 0,
            pre: None,
            build: None,
        };
    }
}

impl PartialOrd for Version {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        match self.major.cmp(&other.major) {
            std::cmp::Ordering::Equal => {}
            x => return Some(x),
        }

        match self.minor.cmp(&other.minor) {
            std::cmp::Ordering::Equal => {}
            x => return Some(x),
        }

        return Some(self.minor.cmp(&other.minor));
    }
}

impl Version {
    pub fn compatible(&self, other: &Self) -> bool {
        return self.major == other.major;
    }

    pub fn to_version_string(&self) -> String {
        let base = format!("{}.{}.{}", self.major, self.minor, self.patch);
        let with_pre = match &self.pre {
            Some(pre) if !pre.is_empty() => format!("{base}-{pre}"),
            _ => base,
        };

        return match &self.build {
            Some(build) if !build.is_empty() => format!("{with_pre}+{build}"),
            _ => with_pre,
        };
    }

    fn from_version_string(input: &str) -> Result<Self> {
        let trimmed = input.trim();
        ensure!(!trimmed.is_empty(), "version string cannot be empty");

        let (pre_build, build) = match trimmed.split_once('+') {
            Some((left, right)) => {
                if right.is_empty() {
                    return Err(anyhow!("build metadata cannot be empty"));
                }
                (left, Some(right.to_string()))
            }
            None => (trimmed, None),
        };

        let (core, pre) = match pre_build.split_once('-') {
            Some((left, right)) => {
                if right.is_empty() {
                    return Err(anyhow!("pre-release cannot be empty"));
                }
                (left, Some(right.to_string()))
            }
            None => (pre_build, None),
        };

        let parts: Vec<&str> = core.split('.').collect();
        if parts.len() != 3 {
            bail!("invalid version string");
        }

        let major = parts[0]
            .parse::<usize>()
            .map_err(|_| anyhow!("invalid major in version: {input}"))?;

        let minor = parts[1]
            .parse::<usize>()
            .map_err(|_| anyhow!("invalid minor in version: {input}"))?;

        let patch = parts[2]
            .parse::<usize>()
            .map_err(|_| anyhow!("invalid patch in version: {input}"))?;

        return Ok(Version {
            major,
            minor,
            patch,
            pre,
            build,
        });
    }
}

impl Serialize for Version {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.to_version_string())
    }
}

impl<'de> Deserialize<'de> for Version {
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let s = String::deserialize(deserializer)?;
        Version::from_version_string(&s).map_err(D::Error::custom)
    }
}
