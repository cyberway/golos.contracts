#!/bin/bash
cleos $url wallet open
cleos $url wallet unlock --password PW5JrNw6R1oXF942LYyoggG7JxbZZiRZQwDDcFAdYQrETh45hCVo7

cleos $url wallet open -n sania
cleos $url wallet unlock -n sania --password PW5KCXCuigS7pwoTYqNAJfBvZ2p9XnW396iQxbWUwv7hHR7MrHKkN
#cleos wallet import -n sania --private-key 5HzCbGTh7H7oyek6Xjv18S4EGwo9rvqtxftTZKHQkLJWwRGymj7
#cleos wallet import -n sania --private-key 5JrTezUCddMBznUwmU3Dvn1KverQDjfp8kB5AutJ6H43QKE2r41

cleos $url create account eosio geos.vest EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio geos.posts EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio geos.token EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio eosio.bios EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio eosio.system EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio pasha EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio sania EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url create account eosio pool EOS8c8a1YWnPUquwmBdHDvn22zfafy9FQWeSpxcXAvytbchYvijDL EOS6irwtUu5oDk5yReDB7didA31wv72hK9AeTcYZpRdGpF5zU4PGN

cleos $url set account permission geos.vest active '{"threshold":1, "accounts":[{"permission":{"actor":"geos.vest","permission":"eosio.code"}, "weight":1}]}' owner -p geos.vest@owner

cleos $url set contract eosio.bios /media/alex/Work/build-eos/contracts/eosio.bios -p eosio.bios
cleos $url set contract eosio.system /media/alex/Work/build-eos/contracts/eosio.system -p eosio.system
cleos $url set contract geos.token /media/alex/Work/build-eos/contracts/eosio.token -p geos.token
cleos $url set contract geos.posts /media/alex/Work/build-eos/contracts/golos.posts -p geos.posts
cleos $url set contract geos.vest /media/alex/Work/build-eos/contracts/golos.vesting -p geos.vest@owner


cleos push action geos.vest createpair '{"vesting" : "0.0000 VEST", "token" : "0.0000 GOLOS"}' -p geos.vest@owner

cleos push action geos.token create '{"issuer":"geos.token", "maximum_supply":"1000000000.0000 GOLOS"}' -p geos.token@owner

cleos push action geos.token issue '[ "geos.token", "10000.0000 GOLOS", "memo" ]' -p geos.token@owner
cleos push action geos.token issue '[ "pasha", "10000.0000 GOLOS", "memo" ]' -p geos.token@owner
cleos push action geos.token issue '[ "sania", "10000.0000 GOLOS", "memo" ]' -p geos.token@owner

cleos push action geos.token transfer '["sania", "geos.vest", "100.0000 GOLOS", ""]' -p sania
cleos push action geos.token transfer '["pasha", "geos.vest", "100.0000 GOLOS", ""]' -p pasha

cleos push action geos.vest accruevg '{"sender":"geos.vest", "user":"sania", "quantity":"12.0000 VEST"}' -p geos.vest@owner
cleos push action geos.vest accruevg '{"sender":"geos.vest", "user":"pasha", "quantity":"12.0000 VEST"}' -p geos.vest@owner

#cleos push action geos.vest convertvg '{"sender":"sania", "recipient":"sania", "quantity":"10.0000 VEST"}' -p sania@owner
#cleos push action geos.vest convertvg '{"sender":"pasha", "recipient":"pasha", "quantity":"10.0000 VEST"}' -p pasha@owner

cleos push action geos.vest delegatevg '{"sender":"sania", "recipient":"pasha", "quantity":"1.0000 VEST", "percentage_deductions":"33.33"}' -p sania
cleos push action geos.vest delegatevg '{"sender":"pasha", "recipient":"sania", "quantity":"7.0000 VEST", "percentage_deductions":"33.33"}' -p pasha


#cleos push action geos.vest undelegatevg '{"sender":"sania", "recipient":"pasha", "quantity":"5.0000 VEST"}' -p sania
#cleos push action geos.vest undelegatevg '{"sender":"pasha", "recipient":"sania", "quantity":"1.0000 VEST"}' -p pasha

#cleos push action geos.vest timeout '{"hash":"1"}' -p geos.vest@owner

