#include "pet_template.h"
#include "Creature.h"
#include "ProgressBar.h"
#include "Database/DatabaseEnv.h"

void pet_template_store::load()
{
    if (!templates_.empty())
        throw std::runtime_error(
            "pet_template_store::load with non-empty templates_");

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT `cid`, `behavior`, `ctemplate_flags`, `pet_flags`, "
        "`behavior_flags`, `spell_dist`, `spell_oom`, `spell_immunity` FROM "
        "`pet_template`"));

    uint32 count = 0;

    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            Field* fields = result->Fetch();
            bar.step();

            pet_template temp;
            temp.cid = fields[0].GetUInt32();
            temp.behavior = fields[1].GetUInt32();
            temp.ctemplate_flags = fields[2].GetUInt32();
            temp.pet_flags = fields[3].GetUInt32();
            temp.behavior_flags = fields[4].GetUInt32();
            temp.spell_dist = fields[5].GetFloat();
            temp.spell_oom = fields[6].GetInt32();
            temp.spell_immunity = fields[7].GetUInt32();

            templates_[temp.cid] = temp;
            ++count;

        } while (result->NextRow());
    }
    logging.info("Loaded %d pet templates.\n", count);

    default_template_ = new pet_template;
    memset(&*default_template_, 0, sizeof(pet_template));
    minipet_template_ = new pet_template;
    memset(&*minipet_template_, 0, sizeof(pet_template));

    // default_template_ left as all zero

    minipet_template_->pet_flags = PET_FLAGS_SPAWN_PASSIVE;
}

const pet_template* pet_template_store::get(unsigned int id) const
{
    auto itr = templates_.find(id);
    if (itr != templates_.end())
        return &(itr->second);
    return default_template_;
}

const pet_template* pet_template_store::get_minipet(unsigned int id) const
{
    auto itr = templates_.find(id);
    if (itr != templates_.end())
        return &(itr->second);
    return minipet_template_;
}

const pet_template* pet_template_store::enslaved_template() const
{
    return default_template_;
}

void pet_template::apply_spell_immunity(Creature* pet) const
{
    if (spell_immunity & 0x0001)
        pet->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, true);
    if (spell_immunity & 0x0002)
        pet->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_HOLY, true);
    if (spell_immunity & 0x0004)
        pet->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_FIRE, true);
    if (spell_immunity & 0x0008)
        pet->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_NATURE, true);
    if (spell_immunity & 0x0010)
        pet->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_FROST, true);
    if (spell_immunity & 0x0020)
        pet->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_SHADOW, true);
    if (spell_immunity & 0x0040)
        pet->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ARCANE, true);
    if (spell_immunity & 0x0080)
        pet->ApplySpellImmune(0, IMMUNITY_DISPEL, DISPEL_MAGIC, true);
    if (spell_immunity & 0x0100)
        pet->ApplySpellImmune(0, IMMUNITY_DISPEL, DISPEL_CURSE, true);
    if (spell_immunity & 0x0200)
        pet->ApplySpellImmune(0, IMMUNITY_DISPEL, DISPEL_DISEASE, true);
    if (spell_immunity & 0x0400)
        pet->ApplySpellImmune(0, IMMUNITY_DISPEL, DISPEL_POISON, true);
}
