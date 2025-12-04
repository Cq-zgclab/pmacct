# my-SAV-ipfix-test

This directory contains a minimal test to validate a draft (non-IANA) set of SAV IPFIX Information Elements using temporary Element IDs (>30000).

What is included
- `nfacctd-00.conf` - minimal pmacct `nfacctd` config that listens on UDP port 9991.
- `send_ipfix.py` - Python script that sends an IPFIX v10 Template and Data record using temporary Element IDs 30001..30004 and Template ID 400.
- `Dockerfile.sender` - tiny image to run the Python sender.
- `docker-compose.yml` - run `nfacctd` (using the public pmacct image) and the sender together.

Run options (Ubuntu cloud host)
Prereqs (on Ubuntu): docker, docker-compose (or use docker compose plugin), python3

1) Using Docker Compose (recommended)

- Copy this test directory to the Ubuntu host, cd into it, then run:

  docker-compose pull; docker-compose up --build --abort-on-container-exit

  Explanation: This will pull the `pmacct/pmacct:latest` image for `nfacctd` and build the sender image, start both containers, run the sender (it sends a packet and exits) and keep `nfacctd` running.

- After the run, fetch the `nfacctd` container logs and paste them here if you want me to analyze the parsing results:

  docker logs nfacctd_sav_test --since 5m

2) Without Docker (run collector from host/source)

- If you have a built `nfacctd` binary (from building pmacct), run it with the provided config:

  sudo /usr/local/sbin/nfacctd -f ./nfacctd-00.conf &

- Run the sender locally (install Python 3):

  python3 send_ipfix.py --host 127.0.0.1 --port 9991

- Check `nfacctd` log output (stdout/stderr or configured log file). Paste relevant portions here and I will iterate.

Notes and assumptions
- The draft's IEs are not IANA-assigned. The sender uses temporary Element IDs 30001..30004 for testing. If you later prefer enterprise bit + PEN encoding, tell me and I will update the template builder.
- The sender currently uses a simplified `savMatchedContentList` (variable-length field with empty payload) for a first-pass parse test. If `nfacctd` shows parsing errors, I will update the script to encode the subTemplateList contents exactly as in the draft.
- If Docker Hub does not have `pmacct/pmacct:latest` for your architecture, you can build pmacct on the host and run `nfacctd` from source; I can provide build steps if needed.

What to paste back here
- The output of `docker logs nfacctd_sav_test --since 5m` (or the collector stdout) after a run.
- If the test failed, paste the `send_ipfix.py` output (it prints send status) and the `nfacctd` logs.

Next steps after you run
- If `nfacctd` accepts the template and data record, I'll update `send_ipfix.py` to encode the `savMatchedContentList` subTemplateList to match the draft's examples (sub-templates 901..904) and move field IDs to enterprise-bit + PEN if you prefer.
