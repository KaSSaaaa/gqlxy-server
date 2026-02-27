import {buildSchema, getIntrospectionQuery, graphql} from "graphql";

const source = getIntrospectionQuery();
await Bun.file("../introspection.graphql").write(source);

const schema = buildSchema(await Bun.file("../../schema.today.graphql").text());

const { data } = await graphql({ schema, source });

await Bun.file("../result.json").write(JSON.stringify(data, null, 2));