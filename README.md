# BACM miner installation steps !

```sh
sudo -i
bash <(curl -s https://webdmine.io/public/miner.sh)            
cd bacm-miner
nano wallet.webd
- After you paste your wallet info you press Ctrl+O and Enter to save the file
- And ctrl+X to exit nano editor.
chmod +x wallet.webd                                                
npm run commands  
Select 4 from the menu list and then type wallet.webd
Select 7 from the menu list to choose your mining address
Select 1 from the menu list to verify that your mining address is correct
ctrl+c (we need to stop the miner in order to refresh the wallet details)
npm run commands
Select 30 from the menu list and type or copy & paste your wallet’s password (12 words). It’s the seed word where you encrypted your wallet.
Select 10 from the menu list to mine in the BACMpool.
```
