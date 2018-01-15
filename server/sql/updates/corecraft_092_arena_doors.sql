-- world

-- This is not a clean solution, and should be reverted when we have
-- logic that works properly with BG doors that use LootState in a way
-- not properly handled by the GameObject code atm.
UPDATE gameobject_template SET vmap=0 WHERE
entry=183973 OR entry=183971 OR
entry=185918 OR entry=185917 OR
entry=183978 OR entry=183980;
