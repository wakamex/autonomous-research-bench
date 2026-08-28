use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{Context, Result, bail};
use serde::Deserialize;
use sha2::{Digest, Sha256};

#[derive(Debug, Clone, Deserialize)]
pub struct Problem {
    pub schema_version: String,
    pub id: String,
    #[serde(rename = "title")]
    pub _title: String,
    pub kind: String,
    pub score_direction: String,
    pub time_limit_seconds: f64,
    pub memory_limit_bytes: u64,
    pub output_limit_bytes: u64,
    pub official_case_count: usize,
    pub compile_time_limit_seconds: f64,
    pub compile_memory_limit_bytes: u64,
    pub public_seeds: Vec<u64>,
    pub package_files: Vec<PathBuf>,
    pub compile: CompileProfile,
    pub tools: Tools,
    pub reference: Option<ReferenceFiles>,
    pub provenance: Provenance,
}

#[derive(Debug, Clone, Deserialize)]
pub struct CompileProfile {
    pub language: String,
    pub program: String,
    pub args: Vec<String>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct Tools {
    pub source: PathBuf,
    pub generator: PathBuf,
    pub tester: PathBuf,
    #[serde(default)]
    pub generator_mode: GeneratorMode,
    #[serde(default)]
    pub judge_mode: JudgeMode,
}

#[derive(Debug, Clone, Copy, Default, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum GeneratorMode {
    #[default]
    DirFlag,
    WorkingDirectory,
}

#[derive(Debug, Clone, Copy, Default, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum JudgeMode {
    #[default]
    Batch,
    Interactive,
}

#[derive(Debug, Clone, Deserialize)]
pub struct ReferenceFiles {
    pub standings: PathBuf,
    pub performance: PathBuf,
}

#[derive(Debug, Clone, Deserialize)]
pub struct Provenance {
    pub source_url: String,
    pub source_sha256: String,
}

#[derive(Debug, Clone)]
pub struct Package {
    pub root: PathBuf,
    pub manifest_hash: String,
    pub problem: Problem,
}

impl Package {
    pub fn load(root: &Path) -> Result<Self> {
        let root = root
            .canonicalize()
            .with_context(|| format!("problem package {}", root.display()))?;
        let manifest_path = root.join("problem.json");
        let raw = fs::read(&manifest_path)
            .with_context(|| format!("read {}", manifest_path.display()))?;
        let problem: Problem = serde_json::from_slice(&raw).context("parse problem.json")?;
        if problem.schema_version != "ars.problem.v1" {
            bail!("unsupported problem schema {}", problem.schema_version);
        }
        if problem.kind != "batch" {
            bail!("only batch problems are currently supported");
        }
        if problem.score_direction != "maximize" {
            bail!("only maximize problems are currently supported");
        }
        if problem.public_seeds.is_empty() {
            bail!("public seed set is empty");
        }
        let manifest_hash = package_hash(&root, &raw, &problem.package_files)?;
        Ok(Self {
            root,
            manifest_hash,
            problem,
        })
    }

    pub fn tools_source(&self) -> PathBuf {
        self.root.join(&self.problem.tools.source)
    }

    pub fn generator(&self) -> PathBuf {
        self.tools_source().join(&self.problem.tools.generator)
    }

    pub fn tester(&self) -> PathBuf {
        self.tools_source().join(&self.problem.tools.tester)
    }

    pub fn standings(&self) -> Result<PathBuf> {
        Ok(self.root.join(
            &self
                .problem
                .reference
                .as_ref()
                .context("problem package has no historical standings")?
                .standings,
        ))
    }

    pub fn performance(&self) -> Result<PathBuf> {
        Ok(self.root.join(
            &self
                .problem
                .reference
                .as_ref()
                .context("problem package has no historical performance mapping")?
                .performance,
        ))
    }

    pub fn build_tools(&self) -> Result<()> {
        let stamp = self
            .tools_source()
            .join("target/release/.ars-package-sha256");
        if self.generator().is_file()
            && self.tester().is_file()
            && fs::read_to_string(&stamp).ok().as_deref() == Some(&self.manifest_hash)
        {
            return Ok(());
        }
        let output = Command::new("cargo")
            .args([
                "build",
                "--release",
                "--locked",
                "--bin",
                "gen",
                "--bin",
                "tester",
            ])
            .current_dir(self.tools_source())
            .env("RUSTFLAGS", "-Awarnings")
            .output()
            .context("start cargo for official tools")?;
        if !output.status.success() {
            bail!(
                "official tool build failed with {}\n{}",
                output.status,
                String::from_utf8_lossy(&output.stderr)
            );
        }
        for path in [self.generator(), self.tester()] {
            if !path.is_file() {
                bail!("official tool was not built: {}", path.display());
            }
        }
        fs::write(stamp, &self.manifest_hash)?;
        Ok(())
    }
}

fn package_hash(root: &Path, manifest: &[u8], files: &[PathBuf]) -> Result<String> {
    let mut digest = Sha256::new();
    digest.update(b"problem.json\0");
    digest.update((manifest.len() as u64).to_le_bytes());
    digest.update(manifest);
    for relative in files {
        if relative.is_absolute()
            || relative
                .components()
                .any(|part| matches!(part, std::path::Component::ParentDir))
        {
            bail!("unsafe package file path {}", relative.display());
        }
        let path = root.join(relative);
        let data =
            fs::read(&path).with_context(|| format!("read package file {}", path.display()))?;
        digest.update(relative.to_string_lossy().as_bytes());
        digest.update(b"\0");
        digest.update((data.len() as u64).to_le_bytes());
        digest.update(data);
    }
    Ok(format!("{:x}", digest.finalize()))
}

pub fn hex_sha256(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

pub fn file_sha256(path: &Path) -> Result<String> {
    Ok(hex_sha256(&fs::read(path)?))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn loads_every_sprint9_package() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR"));
        let ids = [
            "ahc004", "ahc044", "ahc050", "ahc052", "ahc053", "ahc062", "ahc065", "ahc067",
            "ahc068",
        ];
        for id in ids {
            let package = Package::load(&root.join("problems").join(id)).unwrap();
            assert_eq!(package.problem.id, id);
            assert_eq!(package.problem.public_seeds.len(), 50);
            assert!(matches!(package.problem.official_case_count, 100 | 150));
        }
    }

    #[test]
    fn declares_only_the_observed_sprint9_adapter_variants() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR"));
        let ahc004 = Package::load(&root.join("problems/ahc004")).unwrap();
        let ahc053 = Package::load(&root.join("problems/ahc053")).unwrap();
        assert_eq!(
            ahc004.problem.tools.generator_mode,
            GeneratorMode::WorkingDirectory
        );
        assert_eq!(ahc053.problem.tools.judge_mode, JudgeMode::Interactive);
    }
}
