import {buildSchema, getIntrospectionQuery, graphql} from "graphql";

const source = getIntrospectionQuery();
await Bun.file("../introspection.graphql").write(source);

const schema = buildSchema(await Bun.file("../../schema.today.graphql").text());

const { data } = await graphql({ schema, source });

function sortObject(obj: any): any {
    if (Array.isArray(obj)) {
        return obj.map(sortObject);
    } else if (obj !== null && typeof obj === "object") {
        return Object.keys(obj)
            .sort()
            .reduce((acc: any, key) => {
                acc[key] = sortObject(obj[key]);
                return acc;
            }, {});
    }
    return obj;
}

await Bun.file("../result.json").write(JSON.stringify(sortObject(data), null, 2));