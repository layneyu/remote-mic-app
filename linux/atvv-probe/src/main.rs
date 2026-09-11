use anyhow::{Context, Result};
use bluer::{Address, Session, Uuid};
use clap::Parser;

const ATVV_SERVICE: Uuid = Uuid::from_u128(0xab5e0001_5a21_4f05_bc7d_af01f617b664);
const ATVV_TX: Uuid = Uuid::from_u128(0xab5e0002_5a21_4f05_bc7d_af01f617b664);
const ATVV_AUDIO: Uuid = Uuid::from_u128(0xab5e0003_5a21_4f05_bc7d_af01f617b664);
const ATVV_CONTROL: Uuid = Uuid::from_u128(0xab5e0004_5a21_4f05_bc7d_af01f617b664);

fn is_atvv_characteristic(uuid: Uuid) -> bool {
    matches!(uuid, ATVV_TX | ATVV_AUDIO | ATVV_CONTROL)
}

#[derive(Debug, Parser)]
#[command(
    name = "sayall-atvv-probe",
    about = "Read-only BlueZ ATVV capability probe"
)]
struct Cli {
    /// Use a specific BlueZ adapter, for example hci0.
    #[arg(short, long)]
    adapter: Option<String>,

    /// Restrict the probe to one Bluetooth address. The address is not printed.
    #[arg(short = 'd', long)]
    address: Option<String>,
}

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();
    let address_filter = cli
        .address
        .as_deref()
        .map(str::parse::<Address>)
        .transpose()
        .context("invalid Bluetooth address")?;

    println!("probe_started");

    let session = Session::new().await.context("connect to BlueZ session")?;
    let adapter = match cli.adapter.as_deref() {
        Some(name) => session.adapter(name).context("open BlueZ adapter")?,
        None => session
            .default_adapter()
            .await
            .context("find default BlueZ adapter")?,
    };

    let mut found = 0_u32;
    for address in adapter
        .device_addresses()
        .await
        .context("list BlueZ devices")?
    {
        if address_filter.is_some_and(|wanted| wanted != address) {
            continue;
        }

        let device = adapter.device(address)?;
        let uuids = match device.uuids().await {
            Ok(Some(uuids)) => uuids,
            Ok(None) | Err(_) => continue,
        };
        if !uuids.contains(&ATVV_SERVICE) {
            continue;
        }

        found += 1;
        let connected = device.is_connected().await.unwrap_or(false);
        let mut characteristics = 0_u8;
        if connected {
            for service in device.services().await.unwrap_or_default() {
                if service.uuid().await.ok() != Some(ATVV_SERVICE) {
                    continue;
                }
                for characteristic in service.characteristics().await.unwrap_or_default() {
                    let uuid = characteristic.uuid().await.ok();
                    if uuid.is_some_and(is_atvv_characteristic) {
                        characteristics = characteristics.saturating_add(1);
                    }
                }
            }
        }

        println!(
            "atvv_device_found index={found} connected={connected} characteristics={characteristics}"
        );
    }

    println!("probe_complete atvv_devices={found}");
    if found == 0 {
        anyhow::bail!("no ATVV device found in BlueZ known devices");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recognizes_only_the_three_atvv_characteristics() {
        assert!(is_atvv_characteristic(ATVV_TX));
        assert!(is_atvv_characteristic(ATVV_AUDIO));
        assert!(is_atvv_characteristic(ATVV_CONTROL));
        assert!(!is_atvv_characteristic(ATVV_SERVICE));
        assert!(!is_atvv_characteristic(Uuid::nil()));
    }
}
