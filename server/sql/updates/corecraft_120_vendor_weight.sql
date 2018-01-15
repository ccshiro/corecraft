-- world

ALTER TABLE npc_vendor_template
ADD COLUMN weight INT NOT NULL DEFAULT 0;

ALTER TABLE npc_vendor
ADD COLUMN weight INT NOT NULL DEFAULT 0;
